// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Reading, writing, validation and schema migration of simulator profiles.
///
/// Implements `nmeasim::io::Profile` and the name conversions of its enumerations. The
/// helpers in the anonymous namespace convert each section of the JSON document; reading
/// starts from `Profile::default_profile` so that every missing key keeps its default.

#include <nmeasim/io/profile/profile.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>

namespace nmeasim::io {

namespace {

using core::model::FixQuality;
using core::simulation::Variation;

// ---------------------------------------------------------------------------------------------
// Small JSON helpers

/// Reads a number from a JSON object.
///
/// @param object The object to read from.
/// @param key The key, a Latin-1 string literal.
/// @param fallback The value returned when the key is missing or not a number.
/// @return The number stored under `key`, or `fallback`.
double number(const QJsonObject& object, const char* key, double fallback) {
    return object.value(QLatin1String(key)).toDouble(fallback);
}

/// Reads an integer from a JSON object.
///
/// @param object The object to read from.
/// @param key The key, a Latin-1 string literal.
/// @param fallback The value returned when the key is missing, not a number, or a number
///     without an exact `int` representation such as `1.5`.
/// @return The integer stored under `key`, or `fallback`.
int integer(const QJsonObject& object, const char* key, int fallback) {
    return object.value(QLatin1String(key)).toInt(fallback);
}

/// Reads an unsigned integer from a JSON object and checks it against a range.
///
/// A missing key, or a value that is not a number, leaves `value` unchanged, like the other
/// helpers. A number that is negative, larger than `maximum` or not a whole number is
/// rejected instead of being converted, since converting it would wrap or truncate it.
///
/// @param object The object to read from.
/// @param key The key, a Latin-1 string literal.
/// @param name The full name of the key in the profile, such as `simulation.random_seed`,
///     used in the error message.
/// @param maximum The largest value accepted; the range is [0, `maximum`].
/// @param[in,out] value Receives the number read; keeps its value when the key is missing
///     or not a number, and when the number is rejected.
/// @param[out] error Receives `<name> must be a whole number between 0 and <maximum>` when
///     the number is rejected; must not be null.
/// @return False when the number is rejected, true otherwise.
bool unsigned_integer(const QJsonObject& object, const char* key, const QString& name,
                      std::uint32_t maximum, std::uint32_t& value, QString* error) {
    const auto json = object.value(QLatin1String(key));
    if (!json.isDouble()) {
        return true;
    }
    const double number = json.toDouble();
    // The negated comparison also rejects NaN, which a JSON document cannot hold but a
    // QJsonObject built in code can.
    if (!(number >= 0.0 && number <= static_cast<double>(maximum)) ||
        std::trunc(number) != number) {
        *error =
            QStringLiteral("%1 must be a whole number between 0 and %2").arg(name).arg(maximum);
        return false;
    }
    value = static_cast<std::uint32_t>(number);
    return true;
}

/// Reads a boolean from a JSON object.
///
/// @param object The object to read from.
/// @param key The key, a Latin-1 string literal.
/// @param fallback The value returned when the key is missing or not a boolean.
/// @return The boolean stored under `key`, or `fallback`.
bool boolean(const QJsonObject& object, const char* key, bool fallback) {
    return object.value(QLatin1String(key)).toBool(fallback);
}

/// Reads a string from a JSON object.
///
/// @param object The object to read from.
/// @param key The key, a Latin-1 string literal.
/// @param fallback The value returned when the key is missing or not a string; empty by
///     default.
/// @return The string stored under `key`, or `fallback`.
QString text(const QJsonObject& object, const char* key, const QString& fallback = {}) {
    return object.value(QLatin1String(key)).toString(fallback);
}

/// Serialises the drift of one value as an object with `amplitude` and `step_per_second`.
///
/// @param variation The drift, in the unit of the value it applies to.
/// @return The JSON object.
QJsonObject variation_to_json(const Variation& variation) {
    return {{QStringLiteral("amplitude"), variation.amplitude},
            {QStringLiteral("step_per_second"), variation.step_per_second}};
}

/// Reads the drift of one value from an object with `amplitude` and `step_per_second`.
///
/// @param value The JSON value; anything but an object yields `fallback` unchanged.
/// @param fallback The drift used for the whole value or for a missing key.
/// @return The drift read, completed from `fallback`.
Variation variation_from_json(const QJsonValue& value, const Variation& fallback) {
    if (!value.isObject()) {
        return fallback;
    }
    const auto object = value.toObject();
    return {number(object, "amplitude", fallback.amplitude),
            number(object, "step_per_second", fallback.step_per_second)};
}

/// Returns the profile name of a GNSS fix quality, the `simulation.seed.gnss.quality` key.
///
/// @param quality The fix quality.
/// @return `invalid`, `gps` or `differential`; `gps` for a value outside the enumeration.
QString fix_quality_to_string(FixQuality quality) {
    switch (quality) {
        case FixQuality::Invalid:
            return QStringLiteral("invalid");
        case FixQuality::Gps:
            return QStringLiteral("gps");
        case FixQuality::Differential:
            return QStringLiteral("differential");
    }
    return QStringLiteral("gps");
}

/// Parses the profile name of a GNSS fix quality.
///
/// @param value `invalid`, `gps` or `differential`, matched exactly.
/// @return The fix quality, or `std::nullopt` for any other text, which
///     `validate_seed_keys` reports.
std::optional<FixQuality> fix_quality_from_string(const QString& value) {
    for (const auto quality : {FixQuality::Invalid, FixQuality::Gps, FixQuality::Differential}) {
        if (fix_quality_to_string(quality) == value) {
            return quality;
        }
    }
    return std::nullopt;
}

/// Returns the profile name of a serial parity, the `parity` key of a serial output.
///
/// @param parity The parity.
/// @return `none`, `even`, `odd`, `mark` or `space`; `none` for a value outside these.
QString parity_to_string(QSerialPort::Parity parity) {
    switch (parity) {
        case QSerialPort::EvenParity:
            return QStringLiteral("even");
        case QSerialPort::OddParity:
            return QStringLiteral("odd");
        case QSerialPort::MarkParity:
            return QStringLiteral("mark");
        case QSerialPort::SpaceParity:
            return QStringLiteral("space");
        case QSerialPort::NoParity:
            break;
    }
    return QStringLiteral("none");
}

/// Parses the profile name of a serial parity.
///
/// @param value `none`, `even`, `odd`, `mark` or `space`, matched exactly.
/// @return The parity, or `std::nullopt` for any other text, which the caller reports.
std::optional<QSerialPort::Parity> parity_from_string(const QString& value) {
    for (const auto parity :
         {QSerialPort::NoParity, QSerialPort::EvenParity, QSerialPort::OddParity,
          QSerialPort::MarkParity, QSerialPort::SpaceParity}) {
        if (parity_to_string(parity) == value) {
            return parity;
        }
    }
    return std::nullopt;
}

/// Returns the profile name of a number of stop bits, the `stop_bits` key of a serial output.
///
/// @param bits The stop bits.
/// @return `1`, `1.5` or `2`; `1` for a value outside these.
QString stop_bits_to_string(QSerialPort::StopBits bits) {
    switch (bits) {
        case QSerialPort::OneAndHalfStop:
            return QStringLiteral("1.5");
        case QSerialPort::TwoStop:
            return QStringLiteral("2");
        case QSerialPort::OneStop:
            break;
    }
    return QStringLiteral("1");
}

/// Parses the profile name of a number of stop bits.
///
/// @param value `1`, `1.5` or `2`, matched exactly; the key holds a string, not a number.
/// @return The stop bits, or `std::nullopt` for any other text, which the caller reports.
std::optional<QSerialPort::StopBits> stop_bits_from_string(const QString& value) {
    for (const auto bits :
         {QSerialPort::OneStop, QSerialPort::OneAndHalfStop, QSerialPort::TwoStop}) {
        if (stop_bits_to_string(bits) == value) {
            return bits;
        }
    }
    return std::nullopt;
}

/// Returns the profile name of a serial flow control, the `flow_control` key.
///
/// @param flow The flow control.
/// @return `none`, `hardware` (RTS/CTS) or `software` (XON/XOFF); `none` for a value outside
///     these.
QString flow_control_to_string(QSerialPort::FlowControl flow) {
    switch (flow) {
        case QSerialPort::HardwareControl:
            return QStringLiteral("hardware");
        case QSerialPort::SoftwareControl:
            return QStringLiteral("software");
        case QSerialPort::NoFlowControl:
            break;
    }
    return QStringLiteral("none");
}

/// Parses the profile name of a serial flow control.
///
/// @param value `none`, `hardware` or `software`, matched exactly.
/// @return The flow control, or `std::nullopt` for any other text, which the caller reports.
std::optional<QSerialPort::FlowControl> flow_control_from_string(const QString& value) {
    for (const auto flow :
         {QSerialPort::NoFlowControl, QSerialPort::HardwareControl, QSerialPort::SoftwareControl}) {
        if (flow_control_to_string(flow) == value) {
            return flow;
        }
    }
    return std::nullopt;
}

/// Converts the `data_bits` key of a serial output.
///
/// @param bits Bits per character.
/// @return The matching data bits for 5 to 8, or `std::nullopt` for any other value, which
///     the caller reports.
std::optional<QSerialPort::DataBits> data_bits_from_int(int bits) {
    switch (bits) {
        case 5:
            return QSerialPort::Data5;
        case 6:
            return QSerialPort::Data6;
        case 7:
            return QSerialPort::Data7;
        case 8:
            return QSerialPort::Data8;
        default:
            return std::nullopt;
    }
}

/// Returns the profile name of a UDP addressing mode, the `mode` key of a UDP output.
///
/// @param mode The addressing mode.
/// @return `unicast`, `broadcast` or `multicast`, as `to_string(UdpConfig::Mode)` gives it.
QString udp_mode_to_string(UdpConfig::Mode mode) {
    return to_string(mode);
}

/// Parses the profile name of a UDP addressing mode.
///
/// @param value `unicast`, `broadcast` or `multicast`, matched exactly.
/// @return The mode, or `std::nullopt` for any other text, which the caller reports.
std::optional<UdpConfig::Mode> udp_mode_from_string(const QString& value) {
    if (value == QLatin1String("unicast")) {
        return UdpConfig::Mode::Unicast;
    }
    if (value == QLatin1String("broadcast")) {
        return UdpConfig::Mode::Broadcast;
    }
    if (value == QLatin1String("multicast")) {
        return UdpConfig::Mode::Multicast;
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------------------------
// Seed state

/// Serialises the seed values of the vessel as the `simulation.seed` object.
///
/// Only the values a profile stores are written: position, altitude, heading, speed over
/// ground, magnetic variation and deviation, rudder angle, depth, transducer offset, water
/// temperature, true wind, GNSS, engines, destination (`null` when there is none) and AIS
/// static data.
///
/// @param seed The seed state.
/// @return The `simulation.seed` object.
QJsonObject seed_to_json(const core::model::VesselState& seed) {
    const auto& navigation = seed.navigation;
    QJsonObject gnss{
        {QStringLiteral("fix"), seed.gnss.has_fix},
        {QStringLiteral("quality"), fix_quality_to_string(seed.gnss.quality)},
        {QStringLiteral("satellites_in_use"), seed.gnss.satellites_in_use},
        {QStringLiteral("satellites_in_view"), seed.gnss.satellites_in_view},
        {QStringLiteral("hdop"), seed.gnss.hdop},
        {QStringLiteral("pdop"), seed.gnss.pdop},
        {QStringLiteral("vdop"), seed.gnss.vdop},
        {QStringLiteral("geoid_separation_m"), seed.gnss.geoid_separation_m},
    };
    QJsonArray engines;
    for (const auto& engine : seed.engines) {
        engines.append(QJsonObject{
            {QStringLiteral("label"), QString::fromStdString(engine.label)},
            {QStringLiteral("running"), engine.running},
            {QStringLiteral("rpm"), engine.revolutions_rpm},
            {QStringLiteral("coolant_temperature_c"), engine.coolant_temperature_c},
        });
    }
    QJsonObject ais{
        {QStringLiteral("mmsi"), static_cast<qint64>(seed.ais.mmsi)},
        {QStringLiteral("imo_number"), static_cast<qint64>(seed.ais.imo_number)},
        {QStringLiteral("name"), QString::fromStdString(seed.ais.name)},
        {QStringLiteral("call_sign"), QString::fromStdString(seed.ais.call_sign)},
        {QStringLiteral("ship_type"), seed.ais.ship_type},
        {QStringLiteral("dimension_to_bow_m"), seed.ais.dimension_to_bow_m},
        {QStringLiteral("dimension_to_stern_m"), seed.ais.dimension_to_stern_m},
        {QStringLiteral("dimension_to_port_m"), seed.ais.dimension_to_port_m},
        {QStringLiteral("dimension_to_starboard_m"), seed.ais.dimension_to_starboard_m},
        {QStringLiteral("draught_m"), seed.ais.draught_m},
        {QStringLiteral("destination"), QString::fromStdString(seed.ais.destination)},
        {QStringLiteral("navigation_status"), seed.ais.navigation_status},
        {QStringLiteral("position_report_type"), seed.ais.position_report_type},
    };
    QJsonValue destination = QJsonValue::Null;
    if (seed.destination) {
        destination = QJsonObject{
            {QStringLiteral("name"), QString::fromStdString(seed.destination->name)},
            {QStringLiteral("latitude"), seed.destination->position.latitude_deg},
            {QStringLiteral("longitude"), seed.destination->position.longitude_deg},
            {QStringLiteral("origin_latitude"), seed.destination->origin.latitude_deg},
            {QStringLiteral("origin_longitude"), seed.destination->origin.longitude_deg},
            {QStringLiteral("arrival_radius_m"), seed.destination->arrival_radius_m},
        };
    }
    return {
        {QStringLiteral("position"),
         QJsonObject{{QStringLiteral("latitude"), navigation.position.latitude_deg},
                     {QStringLiteral("longitude"), navigation.position.longitude_deg}}},
        {QStringLiteral("altitude_m"), navigation.altitude_m},
        {QStringLiteral("heading_true_deg"), navigation.heading_true_deg},
        {QStringLiteral("speed_over_ground_kn"), navigation.speed_over_ground_kn},
        {QStringLiteral("magnetic_variation_deg"), navigation.magnetic_variation_deg},
        {QStringLiteral("magnetic_deviation_deg"), navigation.magnetic_deviation_deg},
        {QStringLiteral("rudder_angle_deg"), seed.steering.rudder_angle_deg},
        {QStringLiteral("depth_m"), seed.water.depth_below_transducer_m},
        {QStringLiteral("transducer_offset_m"), seed.water.transducer_offset_m},
        {QStringLiteral("water_temperature_c"), seed.water.temperature_c},
        {QStringLiteral("wind_true_direction_deg"), seed.wind.true_direction_deg},
        {QStringLiteral("wind_true_speed_kn"), seed.wind.true_speed_kn},
        {QStringLiteral("gnss"), gnss},
        {QStringLiteral("engines"), engines},
        {QStringLiteral("destination"), destination},
        {QStringLiteral("ais"), ais},
    };
}

/// Reads the `simulation.seed` object on top of a fallback state.
///
/// Every missing key keeps the value of `fallback`. Course over ground is set to the heading
/// and speed through water to the speed over ground, since a profile stores only the latter.
/// A present `engines` key replaces all engines, each missing engine key defaulting to label
/// `Engine`, not running, 0 rpm and 20 degrees Celsius. A present `destination` key sets the
/// destination when it holds an object (latitude and longitude default to 0, the origin to
/// the seed position, the name to `WPT` and the arrival radius to 100 m) and clears it
/// otherwise. The AIS `mmsi` and `imo_number` are left to `ais_identities_from_json`, and
/// nothing is validated here; see `validate_ais`.
///
/// @param object The `simulation.seed` object; empty keeps `fallback` entirely.
/// @param fallback The state the values are read on top of.
/// @return The seed state.
core::model::VesselState seed_from_json(const QJsonObject& object,
                                        const core::model::VesselState& fallback) {
    core::model::VesselState seed = fallback;
    auto& navigation = seed.navigation;
    const auto position = object.value(QStringLiteral("position")).toObject();
    navigation.position.latitude_deg =
        number(position, "latitude", fallback.navigation.position.latitude_deg);
    navigation.position.longitude_deg =
        number(position, "longitude", fallback.navigation.position.longitude_deg);
    navigation.altitude_m = number(object, "altitude_m", fallback.navigation.altitude_m);
    navigation.heading_true_deg =
        number(object, "heading_true_deg", fallback.navigation.heading_true_deg);
    navigation.speed_over_ground_kn =
        number(object, "speed_over_ground_kn", fallback.navigation.speed_over_ground_kn);
    navigation.speed_through_water_kn = navigation.speed_over_ground_kn;
    navigation.course_over_ground_deg = navigation.heading_true_deg;
    navigation.magnetic_variation_deg =
        number(object, "magnetic_variation_deg", fallback.navigation.magnetic_variation_deg);
    navigation.magnetic_deviation_deg =
        number(object, "magnetic_deviation_deg", fallback.navigation.magnetic_deviation_deg);
    seed.steering.rudder_angle_deg =
        number(object, "rudder_angle_deg", fallback.steering.rudder_angle_deg);
    seed.water.depth_below_transducer_m =
        number(object, "depth_m", fallback.water.depth_below_transducer_m);
    seed.water.transducer_offset_m =
        number(object, "transducer_offset_m", fallback.water.transducer_offset_m);
    seed.water.temperature_c = number(object, "water_temperature_c", fallback.water.temperature_c);
    seed.wind.true_direction_deg =
        number(object, "wind_true_direction_deg", fallback.wind.true_direction_deg);
    seed.wind.true_speed_kn = number(object, "wind_true_speed_kn", fallback.wind.true_speed_kn);

    const auto gnss = object.value(QStringLiteral("gnss")).toObject();
    seed.gnss.has_fix = boolean(gnss, "fix", fallback.gnss.has_fix);
    seed.gnss.quality =
        fix_quality_from_string(text(gnss, "quality", fix_quality_to_string(fallback.gnss.quality)))
            .value_or(fallback.gnss.quality);
    seed.gnss.satellites_in_use =
        integer(gnss, "satellites_in_use", fallback.gnss.satellites_in_use);
    seed.gnss.satellites_in_view =
        integer(gnss, "satellites_in_view", fallback.gnss.satellites_in_view);
    seed.gnss.hdop = number(gnss, "hdop", fallback.gnss.hdop);
    seed.gnss.pdop = number(gnss, "pdop", fallback.gnss.pdop);
    seed.gnss.vdop = number(gnss, "vdop", fallback.gnss.vdop);
    seed.gnss.geoid_separation_m =
        number(gnss, "geoid_separation_m", fallback.gnss.geoid_separation_m);

    if (object.contains(QStringLiteral("engines"))) {
        seed.engines.clear();
        const auto engines = object.value(QStringLiteral("engines")).toArray();
        for (const auto& value : engines) {
            const auto engine = value.toObject();
            seed.engines.push_back({text(engine, "label", QStringLiteral("Engine")).toStdString(),
                                    boolean(engine, "running", false), number(engine, "rpm", 0.0),
                                    number(engine, "coolant_temperature_c", 20.0)});
        }
    }
    if (object.contains(QStringLiteral("destination"))) {
        const auto value = object.value(QStringLiteral("destination"));
        if (value.isObject()) {
            const auto destination = value.toObject();
            core::model::Destination result;
            result.name = text(destination, "name", QStringLiteral("WPT")).toStdString();
            result.position = {number(destination, "latitude", 0.0),
                               number(destination, "longitude", 0.0)};
            result.origin = {
                number(destination, "origin_latitude", seed.navigation.position.latitude_deg),
                number(destination, "origin_longitude", seed.navigation.position.longitude_deg)};
            result.arrival_radius_m = number(destination, "arrival_radius_m", 100.0);
            seed.destination = result;
        } else {
            seed.destination.reset();
        }
    }
    const auto ais = object.value(QStringLiteral("ais")).toObject();
    const auto& ais_fallback = fallback.ais;
    seed.ais.name = text(ais, "name", QString::fromStdString(ais_fallback.name)).toStdString();
    seed.ais.call_sign =
        text(ais, "call_sign", QString::fromStdString(ais_fallback.call_sign)).toStdString();
    seed.ais.ship_type = integer(ais, "ship_type", ais_fallback.ship_type);
    seed.ais.dimension_to_bow_m =
        number(ais, "dimension_to_bow_m", ais_fallback.dimension_to_bow_m);
    seed.ais.dimension_to_stern_m =
        number(ais, "dimension_to_stern_m", ais_fallback.dimension_to_stern_m);
    seed.ais.dimension_to_port_m =
        number(ais, "dimension_to_port_m", ais_fallback.dimension_to_port_m);
    seed.ais.dimension_to_starboard_m =
        number(ais, "dimension_to_starboard_m", ais_fallback.dimension_to_starboard_m);
    seed.ais.draught_m = number(ais, "draught_m", ais_fallback.draught_m);
    seed.ais.destination =
        text(ais, "destination", QString::fromStdString(ais_fallback.destination)).toStdString();
    seed.ais.navigation_status = integer(ais, "navigation_status", ais_fallback.navigation_status);
    seed.ais.position_report_type =
        integer(ais, "position_report_type", ais_fallback.position_report_type);
    return seed;
}

/// Reads the MMSI and the IMO number of the `simulation.seed.ais` object.
///
/// Both are whole numbers in [0, 999999999]: nine digits, the length of an MMSI and the
/// largest value the IMO number field of the settings dialog offers, which also fits the
/// 30-bit field of AIS message 5. A missing key keeps the value already in `ais`.
///
/// @param object The `simulation.seed` object.
/// @param[in,out] ais The AIS static data to update.
/// @param[out] error Receives a message naming the offending key when a value is rejected;
///     must not be null.
/// @return False when a value is negative, too large or not a whole number, true otherwise.
/// @see ITU-R M.1371-5, messages 1 and 5.
bool ais_identities_from_json(const QJsonObject& object, core::model::AisStatic& ais,
                              QString* error) {
    constexpr std::uint32_t kNineDigits{999'999'999U};
    const auto json = object.value(QStringLiteral("ais")).toObject();
    return unsigned_integer(json, "mmsi", QStringLiteral("simulation.seed.ais.mmsi"), kNineDigits,
                            ais.mmsi, error) &&
           unsigned_integer(json, "imo_number", QStringLiteral("simulation.seed.ais.imo_number"),
                            kNineDigits, ais.imo_number, error);
}

/// Checks the AIS static data of the own vessel against the ranges of its message fields.
///
/// Checks, in this order: a ship type in [0, 255], a navigational status in [0, 15] and a
/// position report type of 1, 2 or 3. The MMSI and IMO number are checked as they are read,
/// by `ais_identities_from_json`; the dimensions, draught and texts are not checked here.
///
/// @param ais The AIS static data read from the profile.
/// @return A message naming the first offending `simulation.seed.ais` key, or an empty string
///     when the data is valid.
/// @see ITU-R M.1371-5, messages 1, 2, 3 and 5.
QString validate_ais(const core::model::AisStatic& ais) {
    if (ais.ship_type < 0 || ais.ship_type > 255) {
        return QStringLiteral("simulation.seed.ais.ship_type must be between 0 and 255");
    }
    if (ais.navigation_status < 0 || ais.navigation_status > 15) {
        return QStringLiteral("simulation.seed.ais.navigation_status must be between 0 and 15");
    }
    if (ais.position_report_type < 1 || ais.position_report_type > 3) {
        return QStringLiteral("simulation.seed.ais.position_report_type must be 1, 2 or 3");
    }
    return {};
}

/// Checks the keys of the `simulation.seed` object that must hold one of a set of values.
///
/// A `gnss.quality` string must be `invalid`, `gps` or `differential`, and a `destination`
/// object must hold `latitude` and `longitude` as numbers. A key of the wrong JSON type is
/// left to `seed_from_json`, which treats it as missing, except that a destination that is
/// an object always needs its position. `seed_from_json` does not check these keys itself:
/// it keeps the fallback quality for an unknown name and puts a destination without a
/// position at 0 degrees, so this check must pass first.
///
/// @param object The `simulation.seed` object as read from the profile.
/// @return A message naming the first offending `simulation.seed` key, or an empty string when
///     the keys are valid.
QString validate_seed_keys(const QJsonObject& object) {
    const auto quality =
        object.value(QStringLiteral("gnss")).toObject().value(QStringLiteral("quality"));
    if (quality.isString() && !fix_quality_from_string(quality.toString())) {
        return QStringLiteral(
                   "simulation.seed.gnss.quality '%1' is not invalid, gps or differential")
            .arg(quality.toString());
    }
    const auto destination = object.value(QStringLiteral("destination"));
    if (destination.isObject()) {
        for (const char* key : {"latitude", "longitude"}) {
            if (!destination.toObject().value(QLatin1String(key)).isDouble()) {
                return QStringLiteral("simulation.seed.destination needs a numeric %1")
                    .arg(QLatin1String(key));
            }
        }
    }
    return {};
}

/// Checks that a sentence talker is two upper-case letters or empty.
///
/// @param talker The `talker` key of a `sentences.settings` entry.
/// @return True for an empty text, which keeps the registry default, and for two letters
///     from `A` to `Z`; false otherwise.
/// @see NMEA 0183, talker identifier mnemonics.
bool valid_talker(const QString& talker) {
    if (talker.isEmpty()) {
        return true;
    }
    return talker.size() == 2 && std::all_of(talker.begin(), talker.end(), [](QChar character) {
               return character >= QLatin1Char('A') && character <= QLatin1Char('Z');
           });
}

// ---------------------------------------------------------------------------------------------
// Outputs

/// Serialises one output as an entry of the `outputs` array.
///
/// Writes `type`, `enabled`, `encoding` and `filter`, then the keys of the encoding
/// (`tag_block` for NMEA 0183, `period_ms` with `signalk` or `viewsync` otherwise) and those
/// of the transport type. Keys that the type and encoding do not use are not written.
///
/// @param output The output.
/// @return The JSON object.
QJsonObject output_to_json(const OutputConfig& output) {
    QJsonObject object{
        {QStringLiteral("type"), to_string(output.type)},
        {QStringLiteral("enabled"), output.enabled},
        {QStringLiteral("encoding"), to_string(output.encoding)},
        {QStringLiteral("filter"), QJsonArray::fromStringList(output.filter)},
    };
    switch (output.encoding) {
        case OutputConfig::Encoding::Nmea0183:
            object.insert(
                QStringLiteral("tag_block"),
                QJsonObject{
                    {QStringLiteral("enabled"), output.tag_block.enabled},
                    {QStringLiteral("source"),
                     QString::fromStdString(output.tag_block.options.source)},
                    {QStringLiteral("include_time"), output.tag_block.options.include_time},
                    {QStringLiteral("milliseconds"), output.tag_block.options.milliseconds}});
            break;
        case OutputConfig::Encoding::SignalK:
            object.insert(QStringLiteral("period_ms"), output.period_ms);
            object.insert(QStringLiteral("signalk"),
                          QJsonObject{{QStringLiteral("context"),
                                       QString::fromStdString(output.signalk.context)},
                                      {QStringLiteral("source_label"),
                                       QString::fromStdString(output.signalk.source_label)}});
            break;
        case OutputConfig::Encoding::ViewSync:
            object.insert(QStringLiteral("period_ms"), output.period_ms);
            object.insert(
                QStringLiteral("viewsync"),
                QJsonObject{
                    {QStringLiteral("camera_altitude_m"), output.viewsync.camera_altitude_m},
                    {QStringLiteral("tilt_deg"), output.viewsync.tilt_deg},
                    {QStringLiteral("roll_deg"), output.viewsync.roll_deg},
                    {QStringLiteral("planet"), QString::fromStdString(output.viewsync.planet)}});
            break;
    }
    switch (output.type) {
        case OutputConfig::Type::TcpServer:
        case OutputConfig::Type::WebSocketServer:
            object.insert(QStringLiteral("bind_address"), output.bind_address);
            object.insert(QStringLiteral("port"), output.port);
            break;
        case OutputConfig::Type::TcpClient:
            object.insert(QStringLiteral("host"), output.host);
            object.insert(QStringLiteral("port"), output.port);
            object.insert(QStringLiteral("reconnect_ms"), output.reconnect_ms);
            break;
        case OutputConfig::Type::Udp:
            object.insert(QStringLiteral("mode"), udp_mode_to_string(output.udp.mode));
            object.insert(QStringLiteral("address"), output.udp.address);
            object.insert(QStringLiteral("port"), output.udp.port);
            object.insert(QStringLiteral("interface"), output.udp.interface_name);
            object.insert(QStringLiteral("multicast_ttl"), output.udp.multicast_ttl);
            break;
        case OutputConfig::Type::Serial:
            object.insert(QStringLiteral("port_name"), output.serial.port_name);
            object.insert(QStringLiteral("baud_rate"), output.serial.baud_rate);
            object.insert(QStringLiteral("data_bits"), static_cast<int>(output.serial.data_bits));
            object.insert(QStringLiteral("parity"), parity_to_string(output.serial.parity));
            object.insert(QStringLiteral("stop_bits"),
                          stop_bits_to_string(output.serial.stop_bits));
            object.insert(QStringLiteral("flow_control"),
                          flow_control_to_string(output.serial.flow_control));
            break;
        case OutputConfig::Type::File:
        case OutputConfig::Type::Log:
            object.insert(QStringLiteral("path"), output.path);
            object.insert(QStringLiteral("append"), output.append);
            break;
        case OutputConfig::Type::Stdout:
            break;
    }
    return object;
}

/// Reads and validates one entry of the `outputs` array.
///
/// The keys of the encoding are read and validated whatever the encoding. The keys of the
/// transport are read and validated only for the types that use them, so a stray key of
/// another type, such as a `parity` on a TCP server, is ignored. Missing keys take the
/// defaults of `OutputConfig`.
///
/// @param object The entry; a non-object entry arrives as an empty object and fails for its
///     missing `type`.
/// @param index Position of the entry in the array, used in the error message.
/// @param error Receives a message starting with `outputs[index]:` when the entry is
///     invalid; must not be null.
/// @return The output, or `std::nullopt` when the type or encoding is unknown, `period_ms`
///     is out of range, a TCP, UDP or WebSocket `port` is out of range, a TCP client's
///     `reconnect_ms` is outside [1, 3600000], a UDP output has an unknown `mode` or a
///     `multicast_ttl` outside [1, 255], a serial output lacks `port_name`, has a baud rate
///     that is not positive or an unknown `data_bits`, `parity`, `stop_bits` or
///     `flow_control`, or a file or log output lacks `path`.
std::optional<OutputConfig> output_from_json(const QJsonObject& object, int index, QString* error) {
    OutputConfig output;
    const auto type = output_type_from_string(text(object, "type"));
    if (!type) {
        *error =
            QStringLiteral("outputs[%1]: unknown type '%2'").arg(index).arg(text(object, "type"));
        return std::nullopt;
    }
    output.type = *type;
    output.enabled = boolean(object, "enabled", true);
    const auto encoding_text = text(object, "encoding", QStringLiteral("nmea0183"));
    const auto encoding = encoding_from_string(encoding_text);
    if (!encoding) {
        *error =
            QStringLiteral("outputs[%1]: unsupported encoding '%2'").arg(index).arg(encoding_text);
        return std::nullopt;
    }
    output.encoding = *encoding;
    output.period_ms = integer(object, "period_ms", 1000);
    if (output.period_ms < 50 || output.period_ms > 3'600'000) {
        *error = QStringLiteral("outputs[%1]: period_ms must be between 50 and 3600000").arg(index);
        return std::nullopt;
    }
    const auto tag_block = object.value(QStringLiteral("tag_block")).toObject();
    output.tag_block.enabled = boolean(tag_block, "enabled", false);
    output.tag_block.options.source =
        text(tag_block, "source", QStringLiteral("SIM0001")).toStdString();
    output.tag_block.options.include_time = boolean(tag_block, "include_time", true);
    output.tag_block.options.milliseconds = boolean(tag_block, "milliseconds", false);
    const auto signalk = object.value(QStringLiteral("signalk")).toObject();
    output.signalk.context = text(signalk, "context").toStdString();
    output.signalk.source_label =
        text(signalk, "source_label", QStringLiteral("nmeasim")).toStdString();
    const auto viewsync = object.value(QStringLiteral("viewsync")).toObject();
    output.viewsync.camera_altitude_m = number(viewsync, "camera_altitude_m", 500.0);
    output.viewsync.tilt_deg = number(viewsync, "tilt_deg", 60.0);
    output.viewsync.roll_deg = number(viewsync, "roll_deg", 0.0);
    output.viewsync.planet = text(viewsync, "planet").toStdString();
    const auto filter = object.value(QStringLiteral("filter")).toArray();
    for (const auto& value : filter) {
        output.filter.append(value.toString());
    }
    const auto fail = [index, error](const QString& message) {
        *error = QStringLiteral("outputs[%1]: %2").arg(index).arg(message);
        return std::nullopt;
    };
    const bool uses_port = output.type == OutputConfig::Type::TcpServer ||
                           output.type == OutputConfig::Type::TcpClient ||
                           output.type == OutputConfig::Type::Udp ||
                           output.type == OutputConfig::Type::WebSocketServer;
    if (uses_port) {
        const int port = integer(object, "port", 10110);
        if (port < 0 || port > 65535) {
            return fail(QStringLiteral("port %1 is out of range").arg(port));
        }
        output.port = static_cast<quint16>(port);
    }
    output.bind_address = text(object, "bind_address", QStringLiteral("0.0.0.0"));
    output.host = text(object, "host", QStringLiteral("127.0.0.1"));
    if (output.type == OutputConfig::Type::TcpClient) {
        output.reconnect_ms = integer(object, "reconnect_ms", 2000);
        if (output.reconnect_ms < 1 || output.reconnect_ms > 3'600'000) {
            return fail(QStringLiteral("reconnect_ms must be between 1 and 3600000"));
        }
    }

    output.udp.port = output.port;
    if (output.type == OutputConfig::Type::Udp) {
        const auto mode_text = text(object, "mode", QStringLiteral("unicast"));
        const auto mode = udp_mode_from_string(mode_text);
        if (!mode) {
            return fail(QStringLiteral("unknown UDP mode '%1'").arg(mode_text));
        }
        output.udp.mode = *mode;
        output.udp.address = text(object, "address", QStringLiteral("127.0.0.1"));
        output.udp.interface_name = text(object, "interface");
        output.udp.multicast_ttl = integer(object, "multicast_ttl", 1);
        if (output.udp.multicast_ttl < 1 || output.udp.multicast_ttl > 255) {
            return fail(QStringLiteral("multicast_ttl must be between 1 and 255"));
        }
    }

    if (output.type == OutputConfig::Type::Serial) {
        output.serial.port_name = text(object, "port_name");
        if (output.serial.port_name.isEmpty()) {
            return fail(QStringLiteral("serial output needs a port_name"));
        }
        output.serial.baud_rate = integer(object, "baud_rate", 4800);
        if (output.serial.baud_rate <= 0) {
            return fail(QStringLiteral("baud_rate must be positive"));
        }
        const int bits = integer(object, "data_bits", 8);
        const auto data_bits = data_bits_from_int(bits);
        if (!data_bits) {
            return fail(QStringLiteral("data_bits %1 is not 5, 6, 7 or 8").arg(bits));
        }
        output.serial.data_bits = *data_bits;
        const auto parity_text = text(object, "parity", QStringLiteral("none"));
        const auto parity = parity_from_string(parity_text);
        if (!parity) {
            return fail(QStringLiteral("unknown parity '%1'").arg(parity_text));
        }
        output.serial.parity = *parity;
        const auto stop_bits_text = text(object, "stop_bits", QStringLiteral("1"));
        const auto stop_bits = stop_bits_from_string(stop_bits_text);
        if (!stop_bits) {
            return fail(QStringLiteral("unknown stop_bits '%1'").arg(stop_bits_text));
        }
        output.serial.stop_bits = *stop_bits;
        const auto flow_text = text(object, "flow_control", QStringLiteral("none"));
        const auto flow_control = flow_control_from_string(flow_text);
        if (!flow_control) {
            return fail(QStringLiteral("unknown flow_control '%1'").arg(flow_text));
        }
        output.serial.flow_control = *flow_control;
    }

    output.path = text(object, "path");
    output.append = boolean(object, "append", true);
    if ((output.type == OutputConfig::Type::File || output.type == OutputConfig::Type::Log) &&
        output.path.isEmpty()) {
        *error = QStringLiteral("outputs[%1]: %2 output needs a path")
                     .arg(index)
                     .arg(to_string(output.type));
        return std::nullopt;
    }
    return output;
}

// ---------------------------------------------------------------------------------------------
// Migrations

/// Upgrades a profile document from an older schema version to `Profile::kCurrentSchemaVersion`.
///
/// Applies, in order, the step of every version after `from_version`, then sets
/// `schema_version` to the current version. No step so far changes the meaning of an
/// existing key; they at most fill in keys that older files lack.
///
/// @param document The document as read, taken by value and returned modified.
/// @param from_version The document's `schema_version`, 1 or 2.
/// @return The document in the current schema.
QJsonObject migrate(QJsonObject document, int from_version) {
    if (from_version < 2) {
        // Version 2 (milestone M3) added the "track" and "replay" simulation modes with their
        // "simulation.track" and "simulation.replay" objects, and the "log" output type. A
        // version 1 document is always in delta mode and needs no change beyond the marker.
        auto simulation = document.value(QStringLiteral("simulation")).toObject();
        if (!simulation.contains(QStringLiteral("mode"))) {
            simulation.insert(QStringLiteral("mode"), QStringLiteral("delta"));
        }
        document.insert(QStringLiteral("simulation"), simulation);
    }
    if (from_version < 3) {
        // Version 3 (milestone M4) added "simulation.seed.destination" and
        // "simulation.seed.ais", "sentences.custom", and per-output "encoding" values
        // "signalk" and "viewsync" with their "period_ms", "tag_block", "signalk" and
        // "viewsync" objects. Every new key has a default, so a version 2 document loads
        // unchanged; a version 1 or 2 output always carried "nmea0183".
    }
    document.insert(QStringLiteral("schema_version"), Profile::kCurrentSchemaVersion);
    return document;
}

}  // namespace

QString to_string(OutputConfig::Type type) {
    switch (type) {
        case OutputConfig::Type::TcpServer:
            return QStringLiteral("tcp-server");
        case OutputConfig::Type::TcpClient:
            return QStringLiteral("tcp-client");
        case OutputConfig::Type::Udp:
            return QStringLiteral("udp");
        case OutputConfig::Type::WebSocketServer:
            return QStringLiteral("websocket-server");
        case OutputConfig::Type::Serial:
            return QStringLiteral("serial");
        case OutputConfig::Type::File:
            return QStringLiteral("file");
        case OutputConfig::Type::Stdout:
            return QStringLiteral("stdout");
        case OutputConfig::Type::Log:
            return QStringLiteral("log");
    }
    return QStringLiteral("unknown");
}

std::optional<OutputConfig::Type> output_type_from_string(const QString& value) {
    for (const auto type :
         {OutputConfig::Type::TcpServer, OutputConfig::Type::TcpClient, OutputConfig::Type::Udp,
          OutputConfig::Type::WebSocketServer, OutputConfig::Type::Serial, OutputConfig::Type::File,
          OutputConfig::Type::Stdout, OutputConfig::Type::Log}) {
        if (to_string(type) == value) {
            return type;
        }
    }
    return std::nullopt;
}

QString to_string(OutputConfig::Encoding encoding) {
    switch (encoding) {
        case OutputConfig::Encoding::Nmea0183:
            return QStringLiteral("nmea0183");
        case OutputConfig::Encoding::SignalK:
            return QStringLiteral("signalk");
        case OutputConfig::Encoding::ViewSync:
            return QStringLiteral("viewsync");
    }
    return QStringLiteral("nmea0183");
}

std::optional<OutputConfig::Encoding> encoding_from_string(const QString& value) {
    for (const auto encoding : {OutputConfig::Encoding::Nmea0183, OutputConfig::Encoding::SignalK,
                                OutputConfig::Encoding::ViewSync}) {
        if (to_string(encoding) == value) {
            return encoding;
        }
    }
    return std::nullopt;
}

QString to_string(SimulationMode mode) {
    switch (mode) {
        case SimulationMode::Delta:
            return QStringLiteral("delta");
        case SimulationMode::Track:
            return QStringLiteral("track");
        case SimulationMode::Replay:
            return QStringLiteral("replay");
    }
    return QStringLiteral("delta");
}

std::optional<SimulationMode> simulation_mode_from_string(const QString& value) {
    for (const auto mode : {SimulationMode::Delta, SimulationMode::Track, SimulationMode::Replay}) {
        if (to_string(mode) == value) {
            return mode;
        }
    }
    return std::nullopt;
}

Profile Profile::default_profile() {
    Profile profile;
    auto& seed = profile.delta.seed;
    seed.navigation.position = {37.9838, 23.7275};
    seed.navigation.heading_true_deg = 45.0;
    seed.navigation.course_over_ground_deg = 45.0;
    seed.navigation.speed_over_ground_kn = 6.5;
    seed.navigation.speed_through_water_kn = 6.5;
    seed.navigation.magnetic_variation_deg = 4.6;
    seed.water.depth_below_transducer_m = 12.4;
    seed.water.transducer_offset_m = 0.5;
    seed.water.temperature_c = 21.5;
    seed.wind.true_direction_deg = 270.0;
    seed.wind.true_speed_kn = 12.0;
    seed.engines = {{"Port engine", true, 1800.0, 82.0}, {"Starboard engine", true, 1800.0, 83.0}};

    OutputConfig tcp;
    tcp.type = OutputConfig::Type::TcpServer;
    tcp.port = 10110;
    profile.outputs.append(tcp);
    return profile;
}

QJsonObject Profile::to_json() const {
    QJsonObject sentence_settings;
    for (const auto& [id, setting] : sentences) {
        sentence_settings.insert(
            QString::fromStdString(id),
            QJsonObject{
                {QStringLiteral("enabled"), setting.enabled},
                {QStringLiteral("talker"), QString::fromStdString(setting.talker)},
                {QStringLiteral("period_ms"), static_cast<qint64>(setting.period.count())}});
    }
    QJsonArray custom;
    for (const auto& sentence : custom_sentences) {
        custom.append(
            QJsonObject{{QStringLiteral("id"), QString::fromStdString(sentence.id)},
                        {QStringLiteral("body"), QString::fromStdString(sentence.body)},
                        {QStringLiteral("period_ms"), static_cast<qint64>(sentence.period.count())},
                        {QStringLiteral("enabled"), sentence.enabled}});
    }
    QJsonArray output_array;
    for (const auto& output : outputs) {
        output_array.append(output_to_json(output));
    }
    return {
        {QStringLiteral("schema_version"), kCurrentSchemaVersion},
        {QStringLiteral("name"), name},
        {QStringLiteral("simulation"),
         QJsonObject{
             {QStringLiteral("mode"), to_string(mode)},
             {QStringLiteral("tick_ms"), tick_ms},
             {QStringLiteral("start_time"),
              start_time ? start_time->toUTC().toString(Qt::ISODateWithMs) : QStringLiteral("now")},
             {QStringLiteral("random_seed"), static_cast<qint64>(delta.random_seed)},
             {QStringLiteral("seed"), seed_to_json(delta.seed)},
             {QStringLiteral("variation"),
              QJsonObject{
                  {QStringLiteral("heading"), variation_to_json(delta.heading)},
                  {QStringLiteral("speed"), variation_to_json(delta.speed)},
                  {QStringLiteral("depth"), variation_to_json(delta.depth)},
                  {QStringLiteral("water_temperature"), variation_to_json(delta.water_temperature)},
                  {QStringLiteral("wind_direction"), variation_to_json(delta.wind_direction)},
                  {QStringLiteral("wind_speed"), variation_to_json(delta.wind_speed)},
              }},
             {QStringLiteral("steering"),
              QJsonObject{
                  {QStringLiteral("turn_rate_per_rudder_deg"), delta.turn_rate_per_rudder_deg},
                  {QStringLiteral("max_rudder_angle_deg"), delta.max_rudder_angle_deg}}},
             {QStringLiteral("track"),
              QJsonObject{{QStringLiteral("path"), track.path},
                          {QStringLiteral("speed_kn"), track.speed_kn},
                          {QStringLiteral("use_timestamps"), track.use_timestamps},
                          {QStringLiteral("loop"), track.loop}}},
             {QStringLiteral("replay"),
              QJsonObject{{QStringLiteral("path"), replay.path},
                          {QStringLiteral("loop"), replay.loop},
                          {QStringLiteral("fixed_interval_ms"), replay.fixed_interval_ms}}},
         }},
        {QStringLiteral("sentences"),
         QJsonObject{{QStringLiteral("position_decimals"), encoder.position_decimals},
                     {QStringLiteral("settings"), sentence_settings},
                     {QStringLiteral("custom"), custom}}},
        {QStringLiteral("outputs"), output_array},
    };
}

std::optional<Profile> Profile::from_json(const QJsonObject& json, QString* error) {
    QString local_error;
    QString* err = error ? error : &local_error;

    const int version = integer(json, "schema_version", 0);
    if (version < 1) {
        *err = QStringLiteral("Missing or invalid schema_version");
        return std::nullopt;
    }
    if (version > kCurrentSchemaVersion) {
        *err = QStringLiteral("Profile schema version %1 is newer than the supported version %2")
                   .arg(version)
                   .arg(kCurrentSchemaVersion);
        return std::nullopt;
    }
    const QJsonObject document = version < kCurrentSchemaVersion ? migrate(json, version) : json;

    Profile profile = default_profile();
    profile.outputs.clear();
    profile.name = text(document, "name", profile.name);

    const auto simulation = document.value(QStringLiteral("simulation")).toObject();
    const auto mode_text = text(simulation, "mode", QStringLiteral("delta"));
    const auto mode = simulation_mode_from_string(mode_text);
    if (!mode) {
        *err = QStringLiteral("Unsupported simulation mode '%1'").arg(mode_text);
        return std::nullopt;
    }
    profile.mode = *mode;
    profile.tick_ms = integer(simulation, "tick_ms", profile.tick_ms);
    if (profile.tick_ms < 10 || profile.tick_ms > 10'000) {
        *err = QStringLiteral("tick_ms must be between 10 and 10000");
        return std::nullopt;
    }
    const auto start = text(simulation, "start_time", QStringLiteral("now"));
    if (start != QLatin1String("now") && !start.isEmpty()) {
        const auto parsed = QDateTime::fromString(start, Qt::ISODateWithMs);
        if (!parsed.isValid()) {
            *err = QStringLiteral("start_time '%1' is not an ISO 8601 date-time").arg(start);
            return std::nullopt;
        }
        profile.start_time = parsed.toUTC();
    }
    std::uint32_t random_seed{profile.delta.random_seed};
    if (!unsigned_integer(simulation, "random_seed", QStringLiteral("simulation.random_seed"),
                          std::numeric_limits<std::uint32_t>::max(), random_seed, err)) {
        return std::nullopt;
    }
    profile.delta.random_seed = random_seed;
    const auto seed_object = simulation.value(QStringLiteral("seed")).toObject();
    profile.delta.seed = seed_from_json(seed_object, profile.delta.seed);
    if (!ais_identities_from_json(seed_object, profile.delta.seed.ais, err)) {
        return std::nullopt;
    }
    if (const auto problem = validate_ais(profile.delta.seed.ais); !problem.isEmpty()) {
        *err = problem;
        return std::nullopt;
    }
    if (const auto problem =
            validate_seed_keys(simulation.value(QStringLiteral("seed")).toObject());
        !problem.isEmpty()) {
        *err = problem;
        return std::nullopt;
    }
    const auto variation = simulation.value(QStringLiteral("variation")).toObject();
    profile.delta.heading =
        variation_from_json(variation.value(QStringLiteral("heading")), profile.delta.heading);
    profile.delta.speed =
        variation_from_json(variation.value(QStringLiteral("speed")), profile.delta.speed);
    profile.delta.depth =
        variation_from_json(variation.value(QStringLiteral("depth")), profile.delta.depth);
    profile.delta.water_temperature = variation_from_json(
        variation.value(QStringLiteral("water_temperature")), profile.delta.water_temperature);
    profile.delta.wind_direction = variation_from_json(
        variation.value(QStringLiteral("wind_direction")), profile.delta.wind_direction);
    profile.delta.wind_speed = variation_from_json(variation.value(QStringLiteral("wind_speed")),
                                                   profile.delta.wind_speed);
    const auto steering = simulation.value(QStringLiteral("steering")).toObject();
    profile.delta.turn_rate_per_rudder_deg =
        number(steering, "turn_rate_per_rudder_deg", profile.delta.turn_rate_per_rudder_deg);
    profile.delta.max_rudder_angle_deg =
        number(steering, "max_rudder_angle_deg", profile.delta.max_rudder_angle_deg);

    const auto track = simulation.value(QStringLiteral("track")).toObject();
    profile.track.path = text(track, "path");
    profile.track.speed_kn = number(track, "speed_kn", profile.track.speed_kn);
    profile.track.use_timestamps = boolean(track, "use_timestamps", true);
    profile.track.loop = boolean(track, "loop", false);
    if (profile.track.speed_kn <= 0.0) {
        *err = QStringLiteral("simulation.track.speed_kn must be positive");
        return std::nullopt;
    }
    if (profile.mode == SimulationMode::Track && profile.track.path.isEmpty()) {
        *err = QStringLiteral("simulation.track.path is required in track mode");
        return std::nullopt;
    }
    const auto replay = simulation.value(QStringLiteral("replay")).toObject();
    profile.replay.path = text(replay, "path");
    profile.replay.loop = boolean(replay, "loop", false);
    profile.replay.fixed_interval_ms =
        integer(replay, "fixed_interval_ms", profile.replay.fixed_interval_ms);
    if (profile.replay.fixed_interval_ms < 1 || profile.replay.fixed_interval_ms > 60'000) {
        *err = QStringLiteral("simulation.replay.fixed_interval_ms must be between 1 and 60000");
        return std::nullopt;
    }
    if (profile.mode == SimulationMode::Replay && profile.replay.path.isEmpty()) {
        *err = QStringLiteral("simulation.replay.path is required in replay mode");
        return std::nullopt;
    }

    const auto sentences = document.value(QStringLiteral("sentences")).toObject();
    profile.encoder.position_decimals = integer(sentences, "position_decimals", 4);
    if (profile.encoder.position_decimals < 2 || profile.encoder.position_decimals > 8) {
        *err = QStringLiteral("position_decimals must be between 2 and 8");
        return std::nullopt;
    }
    const auto settings = sentences.value(QStringLiteral("settings")).toObject();
    const auto& registry = core::nmea0183::SentenceRegistry::standard();
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        const auto id = it.key().toStdString();
        const auto* descriptor = registry.find(id);
        if (descriptor == nullptr) {
            *err = QStringLiteral("Unknown sentence id '%1' in sentences.settings").arg(it.key());
            return std::nullopt;
        }
        const auto object = it.value().toObject();
        core::simulation::SentenceSetting setting;
        setting.enabled = boolean(object, "enabled", descriptor->enabled_by_default);
        setting.talker = text(object, "talker").toStdString();
        setting.period = std::chrono::milliseconds{
            integer(object, "period_ms", static_cast<int>(descriptor->default_period.count()))};
        if (!valid_talker(QString::fromStdString(setting.talker))) {
            *err =
                QStringLiteral("sentences.settings.%1: talker '%2' is not two upper-case letters")
                    .arg(it.key(), QString::fromStdString(setting.talker));
            return std::nullopt;
        }
        if (setting.period.count() < 50 || setting.period.count() > 3'600'000) {
            *err = QStringLiteral("sentences.settings.%1: period_ms must be between 50 and 3600000")
                       .arg(it.key());
            return std::nullopt;
        }
        profile.sentences[id] = setting;
    }

    const auto custom = sentences.value(QStringLiteral("custom")).toArray();
    for (qsizetype i = 0; i < custom.size(); ++i) {
        const auto object = custom.at(i).toObject();
        core::simulation::CustomSentence sentence;
        sentence.id = text(object, "id").trimmed().toUpper().toStdString();
        sentence.body = text(object, "body").toStdString();
        sentence.period = std::chrono::milliseconds{integer(object, "period_ms", 1000)};
        sentence.enabled = boolean(object, "enabled", true);
        if (const auto problem = core::simulation::validate_custom_sentence(sentence.body)) {
            *err = QStringLiteral("sentences.custom[%1]: %2")
                       .arg(i)
                       .arg(QString::fromStdString(*problem));
            return std::nullopt;
        }
        if (registry.find(sentence.id) != nullptr) {
            *err = QStringLiteral("sentences.custom[%1]: id '%2' is a registry sentence")
                       .arg(i)
                       .arg(QString::fromStdString(sentence.id));
            return std::nullopt;
        }
        if (sentence.period.count() < 50 || sentence.period.count() > 3'600'000) {
            *err = QStringLiteral("sentences.custom[%1]: period_ms must be between 50 and 3600000")
                       .arg(i);
            return std::nullopt;
        }
        profile.custom_sentences.push_back(sentence);
    }
    if (const auto duplicate =
            core::simulation::find_duplicate_custom_id(profile.custom_sentences)) {
        *err = QStringLiteral(
                   "sentences.custom[%1]: id '%2' is already used by another custom "
                   "sentence")
                   .arg(*duplicate)
                   .arg(QString::fromStdString(core::simulation::effective_custom_id(
                       profile.custom_sentences[*duplicate], *duplicate)));
        return std::nullopt;
    }

    const auto outputs = document.value(QStringLiteral("outputs")).toArray();
    for (qsizetype i = 0; i < outputs.size(); ++i) {
        auto output = output_from_json(outputs.at(i).toObject(), static_cast<int>(i), err);
        if (!output) {
            return std::nullopt;
        }
        profile.outputs.append(*output);
    }
    return profile;
}

std::optional<Profile> Profile::load(const QString& path, QString* error) {
    QString local_error;
    QString* err = error ? error : &local_error;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *err = QStringLiteral("Cannot read %1: %2").arg(path, file.errorString());
        return std::nullopt;
    }
    QJsonParseError parse_error{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (document.isNull() || !document.isObject()) {
        *err = QStringLiteral("%1 is not a JSON object: %2").arg(path, parse_error.errorString());
        return std::nullopt;
    }
    auto profile = from_json(document.object(), err);
    if (profile) {
        const QDir directory = QFileInfo(path).absoluteDir();
        for (QString* file_path : {&profile->track.path, &profile->replay.path}) {
            if (!file_path->isEmpty() && QFileInfo(*file_path).isRelative()) {
                *file_path = QDir::cleanPath(directory.absoluteFilePath(*file_path));
            }
        }
    }
    return profile;
}

bool Profile::save(const QString& path, QString* error) const {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    file.write(QJsonDocument(to_json()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    return true;
}

core::simulation::SentenceScheduler Profile::make_scheduler() const {
    core::simulation::SentenceScheduler scheduler;
    scheduler.set_encoder_options(encoder);
    for (const auto& [id, setting] : sentences) {
        scheduler.configure(id, setting);
    }
    scheduler.set_custom_sentences(custom_sentences);
    return scheduler;
}

}  // namespace nmeasim::io
