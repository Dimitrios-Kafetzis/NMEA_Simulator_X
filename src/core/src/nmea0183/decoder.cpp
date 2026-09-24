// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of the NMEA 0183 decoder declared in `decoder.hpp`.
///
/// apply_sentence() dispatches on the formatter to one `decode_` function per sentence type.
/// Each reads the fields it knows, by their zero-based position after the address, and
/// writes only those that parse, so a partial or malformed sentence never clears a value.
/// The number parsers are written by hand rather than with the standard library so that the
/// result does not depend on the process locale.

#include <nmeasim/core/geo/geodesic.hpp>
#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/decoder.hpp>
#include <nmeasim/core/units.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>

namespace nmeasim::core::nmea0183 {

namespace {

/// Removes leading and trailing whitespace, including CR and LF.
///
/// @param text The text to trim.
/// @return A view into `text` without the surrounding whitespace, possibly empty.
std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

/// Tells whether a text consists of ASCII digits only.
///
/// @param text The text to check.
/// @return True when `text` is not empty and every character is `0` to `9`.
bool all_digits(std::string_view text) noexcept {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

/// Returns the value of a string of decimal digits.
///
/// @param text Digits only, few enough for the value to fit an `int`; callers pass at most
///     three.
/// @return The decimal value, 0 for an empty text.
/// @pre all_digits() is true for `text`, or `text` is empty.
int digits_value(std::string_view text) noexcept {
    int value = 0;
    for (const char c : text) {
        value = value * 10 + (c - '0');
    }
    return value;
}

/// Parses a magnitude and its `E` or `W` letter, such as a magnetic variation, into a signed
/// value.
///
/// @param value The magnitude field.
/// @param direction The letter field: `W` makes the value negative; any other letter, or
///     none, leaves it positive.
/// @return The value, positive east; or `std::nullopt` when `value` is empty or malformed.
std::optional<double> signed_east_west(std::string_view value, std::string_view direction) {
    const auto magnitude = parse_number_field(value);
    if (!magnitude) {
        return std::nullopt;
    }
    return direction == "W" ? -*magnitude : *magnitude;
}

/// Sets `state.time_utc` from the time a sentence carries.
///
/// With a date that exists, the date and time are both replaced. Without a date, or with one
/// that does not exist, only the time of day is replaced and the date of the current
/// `state.time_utc` is kept, even when the time of day has wrapped past midnight.
///
/// @param[in,out] state The state whose `time_utc` is set.
/// @param time The time of day and optional date read by sentence_time().
void apply_time(model::VesselState& state, const SentenceTime& time) {
    using namespace std::chrono;
    if (time.date) {
        const year_month_day date{year{time.date->year},
                                  month{static_cast<unsigned>(time.date->month)},
                                  day{static_cast<unsigned>(time.date->day)}};
        if (date.ok()) {
            state.time_utc = sys_days{date} + time.since_midnight;
            return;
        }
    }
    state.time_utc = floor<days>(state.time_utc) + time.since_midnight;
}

/// Reads a latitude, its hemisphere, a longitude and its hemisphere from four consecutive
/// fields into the vessel position.
///
/// The position is updated only when both coordinates parse, so a sentence without a fix
/// leaves the last position in place rather than moving the vessel to zero.
///
/// @param sentence The parsed sentence.
/// @param first_field Zero-based index of the latitude field.
/// @param[in,out] state The state whose `navigation.position` is set.
void apply_position(const ParsedSentence& sentence, std::size_t first_field,
                    model::VesselState& state) {
    const auto latitude =
        parse_coordinate(sentence.field(first_field), sentence.field(first_field + 1));
    const auto longitude =
        parse_coordinate(sentence.field(first_field + 2), sentence.field(first_field + 3));
    if (latitude && longitude) {
        state.navigation.position = {*latitude, *longitude};
    }
}

/// Assigns a decoded value when there is one.
///
/// @tparam T The type of the value.
/// @param value The decoded value, `std::nullopt` when the field was empty or malformed.
/// @param[out] target The state member to set; left unchanged when `value` is empty.
template <typename T>
void assign(std::optional<T> value, T& target) {
    if (value) {
        target = *value;
    }
}

/// Parses a wind speed and its unit letter into knots.
///
/// @param value The speed field.
/// @param unit The unit field: `K` km/h, `M` metres per second, `S` statute miles per hour;
///     `N` and anything else are taken as knots.
/// @return The speed in knots, or `std::nullopt` when `value` is empty or malformed.
std::optional<double> wind_speed_knots(std::string_view value, std::string_view unit) {
    const auto speed = parse_number_field(value);
    if (!speed) {
        return std::nullopt;
    }
    if (unit == "K") {
        return *speed / units::kKilometresPerHourPerKnot;
    }
    if (unit == "M") {
        return units::mps_to_knots(*speed);
    }
    if (unit == "S") {
        return *speed / 1.15078;  // Statute miles per hour in one knot, 1852 m / 1609.344 m.
    }
    return *speed;
}

/// Applies RMC: fix status, time and date, position, speed and course over ground, magnetic
/// variation and the fix quality implied by the mode indicator.
///
/// Status `A` or `V` sets `gnss.has_fix`. Mode `D` sets a differential fix and mode `A` a
/// GPS fix; other modes leave the quality alone.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence RMC.
bool decode_rmc(const ParsedSentence& s, model::VesselState& state) {
    const auto status = s.field(1);
    if (status == "A" || status == "V") {
        state.gnss.has_fix = status == "A";
    }
    if (const auto time = sentence_time(s)) {
        apply_time(state, *time);
    }
    apply_position(s, 2, state);
    assign(parse_number_field(s.field(6)), state.navigation.speed_over_ground_kn);
    assign(parse_number_field(s.field(7)), state.navigation.course_over_ground_deg);
    assign(signed_east_west(s.field(9), s.field(10)), state.navigation.magnetic_variation_deg);
    if (s.field(11) == "D") {
        state.gnss.quality = model::FixQuality::Differential;
    } else if (s.field(11) == "A") {
        state.gnss.quality = model::FixQuality::Gps;
    }
    return true;
}

/// Applies GGA: time of day, position, fix quality, satellites in use, HDOP, altitude and
/// geoid separation.
///
/// Quality `0` clears `gnss.has_fix` and sets model::FixQuality::Invalid, `2` sets a
/// differential fix and any other code a GPS fix. The satellites in view are raised to at
/// least the satellites in use. Altitude and geoid separation are taken as metres without
/// checking their unit fields.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence GGA.
bool decode_gga(const ParsedSentence& s, model::VesselState& state) {
    if (const auto time = sentence_time(s)) {
        apply_time(state, *time);
    }
    apply_position(s, 1, state);
    if (const auto quality = parse_number_field(s.field(5))) {
        const int code = static_cast<int>(*quality);
        state.gnss.has_fix = code != 0;
        state.gnss.quality = code == 0   ? model::FixQuality::Invalid
                             : code == 2 ? model::FixQuality::Differential
                                         : model::FixQuality::Gps;
    }
    if (const auto satellites = parse_number_field(s.field(6))) {
        state.gnss.satellites_in_use = static_cast<int>(*satellites);
        state.gnss.satellites_in_view =
            std::max(state.gnss.satellites_in_view, state.gnss.satellites_in_use);
    }
    assign(parse_number_field(s.field(7)), state.gnss.hdop);
    assign(parse_number_field(s.field(8)), state.navigation.altitude_m);
    assign(parse_number_field(s.field(10)), state.gnss.geoid_separation_m);
    return true;
}

/// Applies GLL: position, time of day and fix status (`A` or `V`); the mode is ignored.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence GLL.
bool decode_gll(const ParsedSentence& s, model::VesselState& state) {
    apply_position(s, 0, state);
    if (const auto time = sentence_time(s)) {
        apply_time(state, *time);
    }
    const auto status = s.field(5);
    if (status == "A" || status == "V") {
        state.gnss.has_fix = status == "A";
    }
    return true;
}

/// Applies GSA: fix type, satellites in use and PDOP, HDOP and VDOP.
///
/// Fix type 2 (2D) or 3 (3D) sets `gnss.has_fix`, 1 clears it. The satellites in use are the
/// number of non-empty PRN slots, applied only when at least one is filled; the satellites
/// in view are raised to at least that number.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence GSA.
bool decode_gsa(const ParsedSentence& s, model::VesselState& state) {
    if (const auto fix_type = parse_number_field(s.field(1))) {
        state.gnss.has_fix = *fix_type >= 2.0;
    }
    int in_use = 0;
    for (std::size_t i = 2; i < 14; ++i) {
        if (!s.field(i).empty()) {
            ++in_use;
        }
    }
    if (in_use > 0) {
        state.gnss.satellites_in_use = in_use;
        state.gnss.satellites_in_view = std::max(state.gnss.satellites_in_view, in_use);
    }
    assign(parse_number_field(s.field(14)), state.gnss.pdop);
    assign(parse_number_field(s.field(15)), state.gnss.hdop);
    assign(parse_number_field(s.field(16)), state.gnss.vdop);
    return true;
}

/// Applies GSV: the number of satellites in view; the per-satellite blocks are ignored.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence GSV.
bool decode_gsv(const ParsedSentence& s, model::VesselState& state) {
    if (const auto in_view = parse_number_field(s.field(2))) {
        state.gnss.satellites_in_view = static_cast<int>(*in_view);
    }
    return true;
}

/// Applies VTG: course over ground in degrees true and speed over ground in knots.
///
/// The magnetic course, the speed in km/h and the mode are ignored.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence VTG.
bool decode_vtg(const ParsedSentence& s, model::VesselState& state) {
    assign(parse_number_field(s.field(0)), state.navigation.course_over_ground_deg);
    assign(parse_number_field(s.field(4)), state.navigation.speed_over_ground_kn);
    return true;
}

/// Applies ZDA: the UTC time and date; the local zone is ignored.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence ZDA.
bool decode_zda(const ParsedSentence& s, model::VesselState& state) {
    if (const auto time = sentence_time(s)) {
        apply_time(state, *time);
    }
    return true;
}

/// Applies HDT: the true heading, normalised to [0, 360).
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence HDT.
bool decode_hdt(const ParsedSentence& s, model::VesselState& state) {
    if (const auto heading = parse_number_field(s.field(0))) {
        state.navigation.heading_true_deg = geo::normalize_bearing(*heading);
    }
    return true;
}

/// Applies HDG: deviation, variation and the heading.
///
/// Deviation and variation are applied first, positive east; the true heading is then the
/// sensor heading plus variation plus deviation, normalised to [0, 360), using the values
/// just read or the previous ones where those fields are empty.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence HDG.
bool decode_hdg(const ParsedSentence& s, model::VesselState& state) {
    assign(signed_east_west(s.field(1), s.field(2)), state.navigation.magnetic_deviation_deg);
    assign(signed_east_west(s.field(3), s.field(4)), state.navigation.magnetic_variation_deg);
    if (const auto magnetic = parse_number_field(s.field(0))) {
        state.navigation.heading_true_deg =
            geo::normalize_bearing(*magnetic + state.navigation.magnetic_variation_deg +
                                   state.navigation.magnetic_deviation_deg);
    }
    return true;
}

/// Applies HDM: the heading, converted to true with the variation and deviation already in
/// the state.
///
/// The true heading is the sent heading plus variation plus deviation, normalised to
/// [0, 360), the inverse of what encode_hdm() sends.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence HDM.
bool decode_hdm(const ParsedSentence& s, model::VesselState& state) {
    if (const auto magnetic = parse_number_field(s.field(0))) {
        state.navigation.heading_true_deg =
            geo::normalize_bearing(*magnetic + state.navigation.magnetic_variation_deg +
                                   state.navigation.magnetic_deviation_deg);
    }
    return true;
}

/// Applies ROT: the rate of turn in degrees per minute, negative to port; the status field is
/// not checked.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence ROT.
bool decode_rot(const ParsedSentence& s, model::VesselState& state) {
    assign(parse_number_field(s.field(0)), state.navigation.rate_of_turn_deg_per_min);
    return true;
}

/// Applies VHW: the true heading, normalised to [0, 360), and the speed through the water in
/// knots.
///
/// The magnetic heading and the speed in km/h are ignored.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence VHW.
bool decode_vhw(const ParsedSentence& s, model::VesselState& state) {
    if (const auto heading = parse_number_field(s.field(0))) {
        state.navigation.heading_true_deg = geo::normalize_bearing(*heading);
    }
    assign(parse_number_field(s.field(4)), state.navigation.speed_through_water_kn);
    return true;
}

/// Applies VBW: the longitudinal water speed, unless its status is `V`.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence VBW.
bool decode_vbw(const ParsedSentence& s, model::VesselState& state) {
    // Only the water speed is taken: the ground speed is resolved onto the vessel's axes
    // and rounded, so RMC and VTG carry it more precisely.
    if (s.field(2) != "V") {
        assign(parse_number_field(s.field(0)), state.navigation.speed_through_water_kn);
    }
    return true;
}

/// Applies DPT: the depth below the transducer and the transducer offset, both in metres.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence DPT.
bool decode_dpt(const ParsedSentence& s, model::VesselState& state) {
    assign(parse_number_field(s.field(0)), state.water.depth_below_transducer_m);
    assign(parse_number_field(s.field(1)), state.water.transducer_offset_m);
    return true;
}

/// Applies DBT: the depth below the transducer, from the metres field or, when that is
/// empty, from the feet field; the fathoms field is ignored.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence DBT.
bool decode_dbt(const ParsedSentence& s, model::VesselState& state) {
    if (const auto metres = parse_number_field(s.field(2))) {
        state.water.depth_below_transducer_m = *metres;
    } else if (const auto feet = parse_number_field(s.field(0))) {
        state.water.depth_below_transducer_m = *feet / units::kFeetPerMetre;
    }
    return true;
}

/// Applies MTW: the water temperature, taken as degrees Celsius without checking the unit.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence MTW.
bool decode_mtw(const ParsedSentence& s, model::VesselState& state) {
    assign(parse_number_field(s.field(0)), state.water.temperature_c);
    return true;
}

/// Applies MWV: the true or the apparent wind, depending on the reference field.
///
/// A sentence with status `V` is ignored. With reference `T` the angle relative to the bow
/// is added to the current true heading to give the true wind direction, and the speed sets
/// the true wind speed. Any other reference, normally `R`, sets the apparent wind angle,
/// normalised to [0, 360), and speed. Speeds in km/h, metres per second and statute miles
/// per hour are converted to knots.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence MWV.
bool decode_mwv(const ParsedSentence& s, model::VesselState& state) {
    if (s.field(4) == "V") {
        return true;
    }
    const auto angle = parse_number_field(s.field(0));
    const auto speed = wind_speed_knots(s.field(2), s.field(3));
    if (s.field(1) == "T") {
        if (angle) {
            state.wind.true_direction_deg =
                geo::normalize_bearing(state.navigation.heading_true_deg + *angle);
        }
        assign(speed, state.wind.true_speed_kn);
    } else {
        if (angle) {
            state.wind.apparent_angle_deg = geo::normalize_bearing(*angle);
        }
        assign(speed, state.wind.apparent_speed_kn);
    }
    return true;
}

/// Applies MWD: the true wind direction, normalised to [0, 360), and the true wind speed.
///
/// The speed is read in knots or, when that field is empty, in metres per second. The
/// magnetic direction is ignored.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence MWD.
bool decode_mwd(const ParsedSentence& s, model::VesselState& state) {
    if (const auto direction = parse_number_field(s.field(0))) {
        state.wind.true_direction_deg = geo::normalize_bearing(*direction);
    }
    if (const auto knots = parse_number_field(s.field(4))) {
        state.wind.true_speed_kn = *knots;
    } else if (const auto mps = parse_number_field(s.field(6))) {
        state.wind.true_speed_kn = units::mps_to_knots(*mps);
    }
    return true;
}

/// Accepts APB and XTE without changing the state.
///
/// Both repeat what RMB carries but without the destination position, so they are
/// recognised, which makes apply_sentence() return true, and otherwise ignored. The sentence
/// and the state are not used.
///
/// @return Always true.
/// @see NMEA 0183, sentences APB and XTE.
bool decode_nothing(const ParsedSentence& /*s*/, model::VesselState& /*state*/) {
    return true;
}

/// Applies RMB: the destination name and position.
///
/// Only a sentence with status `A` and a destination position that parses is applied. An
/// empty name becomes `WPT` before the comparison, so it matches a current destination named
/// `WPT`. For the same name as the current destination the origin and the arrival radius are
/// kept; for a new destination the leg starts at the vessel's current
/// position and the arrival radius takes its default. Cross-track error, range, bearing and
/// closing velocity are ignored, since they follow from the positions.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence RMB.
bool decode_rmb(const ParsedSentence& s, model::VesselState& state) {
    if (s.field(0) != "A") {
        return true;
    }
    const auto latitude = parse_coordinate(s.field(5), s.field(6));
    const auto longitude = parse_coordinate(s.field(7), s.field(8));
    if (!latitude || !longitude) {
        return true;
    }
    // An empty name stands for WPT, so it is compared as such with the current destination.
    const std::string name = s.field(4).empty() ? std::string{"WPT"} : std::string{s.field(4)};
    // The sentence does not carry the origin: a new destination starts its leg where the
    // vessel is, an update of the same destination keeps the leg.
    const bool same = state.destination && state.destination->name == name;
    model::Destination destination;
    destination.name = name;
    destination.position = {*latitude, *longitude};
    destination.origin = same ? state.destination->origin : state.navigation.position;
    if (same) {
        destination.arrival_radius_m = state.destination->arrival_radius_m;
    }
    state.destination = destination;
    return true;
}

/// Returns the engine at an index, adding engines as needed.
///
/// Missing engines are appended with default values and the labels `Engine 1`, `Engine 2`,
/// and so on, numbered by their position.
///
/// @param[in,out] state The state whose `engines` may grow.
/// @param index Zero-based engine index; callers keep it below 100.
/// @return The engine, valid until `state.engines` is next resized.
model::Engine& engine_at(model::VesselState& state, std::size_t index) {
    while (state.engines.size() <= index) {
        model::Engine engine;
        engine.label = "Engine " + std::to_string(state.engines.size() + 1);
        state.engines.push_back(engine);
    }
    return state.engines[index];
}

/// Returns the index of the engine named by an XDR transducer id.
///
/// @param transducer The transducer id field, such as `ENGINE#1`.
/// @return The number after `ENGINE#`, which counts engines from 0; or `std::nullopt` when
///     the id has another form or the number is not one to three digits.
std::optional<std::size_t> engine_index(std::string_view transducer) {
    constexpr std::string_view kPrefix{"ENGINE#"};
    if (!transducer.starts_with(kPrefix)) {
        return std::nullopt;
    }
    const auto digits = transducer.substr(kPrefix.size());
    if (!all_digits(digits) || digits.size() > 3) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(digits_value(digits));
}

/// Applies RPM: the revolutions of one engine.
///
/// Only a sentence with source `E` (engine, not shaft), status `A` and an engine number in
/// [1, 100] is applied. Engine `n` is `state.engines[n - 1]`, added if missing; it is running
/// when the revolutions are above zero.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence RPM.
bool decode_rpm(const ParsedSentence& s, model::VesselState& state) {
    if (s.field(4) != "A" || s.field(0) != "E") {
        return true;
    }
    const auto number = parse_number_field(s.field(1));
    const auto rpm = parse_number_field(s.field(2));
    if (!number || !rpm || *number < 1.0 || *number > 100.0) {
        return true;
    }
    auto& engine = engine_at(state, static_cast<std::size_t>(*number) - 1);
    engine.revolutions_rpm = *rpm;
    engine.running = *rpm > 0.0;
    return true;
}

/// Applies XDR: coolant temperatures and revolutions of engines.
///
/// The fields are read in groups of four: type, value, unit and transducer id. A group with
/// an id `ENGINE#n`, `n` in [0, 99], sets the coolant temperature of `state.engines[n]` for
/// type `C` with unit `C`, and its revolutions and running flag for type `T` with unit `R`;
/// engines are added if missing. Other groups are ignored.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence XDR.
bool decode_xdr(const ParsedSentence& s, model::VesselState& state) {
    for (std::size_t i = 0; i + 3 < s.fields.size(); i += 4) {
        const auto index = engine_index(s.field(i + 3));
        const auto value = parse_number_field(s.field(i + 1));
        if (!index || !value || *index >= 100) {
            continue;
        }
        if (s.field(i) == "C" && s.field(i + 2) == "C") {
            engine_at(state, *index).coolant_temperature_c = *value;
        } else if (s.field(i) == "T" && s.field(i + 2) == "R") {
            auto& engine = engine_at(state, *index);
            engine.revolutions_rpm = *value;
            engine.running = *value > 0.0;
        }
    }
    return true;
}

/// Applies RSA: the starboard (or single) rudder angle, positive to starboard, when its status
/// is `A`; the port rudder is ignored.
///
/// @param s The parsed sentence; the talker is ignored.
/// @param[in,out] state The state to update.
/// @return Always true: the formatter is recognised even when no field could be used.
/// @see NMEA 0183, sentence RSA.
bool decode_rsa(const ParsedSentence& s, model::VesselState& state) {
    if (s.field(1) == "A") {
        assign(parse_number_field(s.field(0)), state.steering.rudder_angle_deg);
    }
    return true;
}

}  // namespace

std::string_view ParsedSentence::field(std::size_t index) const noexcept {
    return index < fields.size() ? std::string_view{fields[index]} : std::string_view{};
}

std::optional<ParsedSentence> parse_sentence(std::string_view line) {
    line = trim(line);
    if (line.size() < 2 ||
        (line.front() != kStartDelimiter && line.front() != kEncapsulationDelimiter)) {
        return std::nullopt;
    }
    ParsedSentence sentence;
    sentence.delimiter = line.front();
    std::string_view body = line.substr(1);
    const auto star = body.find(kChecksumDelimiter);
    if (star != std::string_view::npos) {
        if (!verify_checksum(line)) {
            return std::nullopt;
        }
        sentence.has_checksum = true;
        body = body.substr(0, star);
    }
    const auto comma = body.find(',');
    const std::string_view address = body.substr(0, comma);
    if (address.size() < 3) {
        return std::nullopt;
    }
    const std::size_t talker_length = address.front() == 'P' ? 1 : 2;
    sentence.talker = std::string{address.substr(0, talker_length)};
    sentence.formatter = std::string{address.substr(talker_length)};
    if (sentence.formatter.empty()) {
        return std::nullopt;
    }
    if (comma != std::string_view::npos) {
        std::string_view rest = body.substr(comma + 1);
        while (true) {
            const auto next = rest.find(',');
            sentence.fields.emplace_back(rest.substr(0, next));
            if (next == std::string_view::npos) {
                break;
            }
            rest = rest.substr(next + 1);
        }
    }
    return sentence;
}

std::optional<double> parse_number_field(std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        return std::nullopt;
    }
    std::size_t offset = 0;
    double sign = 1.0;
    if (value[offset] == '-' || value[offset] == '+') {
        sign = value[offset] == '-' ? -1.0 : 1.0;
        ++offset;
    }
    double result = 0.0;
    bool any_digit = false;
    while (offset < value.size() && value[offset] >= '0' && value[offset] <= '9') {
        result = result * 10.0 + (value[offset] - '0');
        any_digit = true;
        ++offset;
    }
    if (offset < value.size() && value[offset] == '.') {
        ++offset;
        double scale = 0.1;
        while (offset < value.size() && value[offset] >= '0' && value[offset] <= '9') {
            result += (value[offset] - '0') * scale;
            scale /= 10.0;
            any_digit = true;
            ++offset;
        }
    }
    if (!any_digit || offset != value.size()) {
        return std::nullopt;
    }
    return sign * result;
}

std::optional<double> parse_coordinate(std::string_view value, std::string_view hemisphere) {
    const auto number = parse_number_field(value);
    if (!number || hemisphere.size() != 1) {
        return std::nullopt;
    }
    const double degrees = std::floor(*number / 100.0);
    const double minutes = *number - degrees * 100.0;
    if (minutes >= 60.0) {
        return std::nullopt;
    }
    double result = degrees + minutes / 60.0;
    switch (hemisphere.front()) {
        case 'N':
            return result <= 90.0 ? std::optional{result} : std::nullopt;
        case 'S':
            return result <= 90.0 ? std::optional{-result} : std::nullopt;
        case 'E':
            return result <= 180.0 ? std::optional{result} : std::nullopt;
        case 'W':
            return result <= 180.0 ? std::optional{-result} : std::nullopt;
        default:
            return std::nullopt;
    }
}

std::optional<std::chrono::milliseconds> parse_time_of_day(std::string_view value) {
    value = trim(value);
    const auto dot = value.find('.');
    const std::string_view whole = value.substr(0, dot);
    if (whole.size() != 6 || !all_digits(whole)) {
        return std::nullopt;
    }
    const int hours = digits_value(whole.substr(0, 2));
    const int minutes = digits_value(whole.substr(2, 2));
    const int seconds = digits_value(whole.substr(4, 2));
    if (hours > 23 || minutes > 59 || seconds > 60) {
        return std::nullopt;
    }
    int millis = 0;
    if (dot != std::string_view::npos) {
        const std::string_view fraction = value.substr(dot + 1);
        if (!all_digits(fraction)) {
            return std::nullopt;
        }
        int scale = 100;
        for (std::size_t i = 0; i < fraction.size() && i < 3; ++i) {
            millis += (fraction[i] - '0') * scale;
            scale /= 10;
        }
    }
    return std::chrono::milliseconds{((hours * 60 + minutes) * 60 + seconds) * 1000 + millis};
}

std::optional<DateParts> parse_date(std::string_view value) {
    value = trim(value);
    if (value.size() != 6 || !all_digits(value)) {
        return std::nullopt;
    }
    const int day = digits_value(value.substr(0, 2));
    const int month = digits_value(value.substr(2, 2));
    const int year = digits_value(value.substr(4, 2));
    if (day < 1 || day > 31 || month < 1 || month > 12) {
        return std::nullopt;
    }
    return DateParts{year < 80 ? 2000 + year : 1900 + year, month, day};
}

std::optional<SentenceTime> sentence_time(const ParsedSentence& sentence) {
    const auto& formatter = sentence.formatter;
    std::size_t time_field = 0;
    if (formatter == "GLL") {
        time_field = 4;
    } else if (formatter != "RMC" && formatter != "GGA" && formatter != "ZDA" &&
               formatter != "GNS" && formatter != "GST" && formatter != "GBS" &&
               formatter != "GRS") {
        return std::nullopt;
    }
    const auto time_of_day = parse_time_of_day(sentence.field(time_field));
    if (!time_of_day) {
        return std::nullopt;
    }
    SentenceTime result{*time_of_day, std::nullopt};
    if (formatter == "RMC") {
        result.date = parse_date(sentence.field(8));
    } else if (formatter == "ZDA") {
        const auto day = parse_number_field(sentence.field(1));
        const auto month = parse_number_field(sentence.field(2));
        const auto year = parse_number_field(sentence.field(3));
        if (day && month && year) {
            result.date = DateParts{static_cast<int>(*year), static_cast<int>(*month),
                                    static_cast<int>(*day)};
        }
    }
    return result;
}

bool apply_sentence(const ParsedSentence& sentence, model::VesselState& state) {
    if (sentence.delimiter != kStartDelimiter) {
        return false;
    }
    const auto& f = sentence.formatter;
    if (f == "RMC") {
        return decode_rmc(sentence, state);
    }
    if (f == "GGA") {
        return decode_gga(sentence, state);
    }
    if (f == "GLL") {
        return decode_gll(sentence, state);
    }
    if (f == "GSA") {
        return decode_gsa(sentence, state);
    }
    if (f == "GSV") {
        return decode_gsv(sentence, state);
    }
    if (f == "VTG") {
        return decode_vtg(sentence, state);
    }
    if (f == "ZDA") {
        return decode_zda(sentence, state);
    }
    if (f == "HDT") {
        return decode_hdt(sentence, state);
    }
    if (f == "HDG") {
        return decode_hdg(sentence, state);
    }
    if (f == "HDM") {
        return decode_hdm(sentence, state);
    }
    if (f == "ROT") {
        return decode_rot(sentence, state);
    }
    if (f == "VHW") {
        return decode_vhw(sentence, state);
    }
    if (f == "VBW") {
        return decode_vbw(sentence, state);
    }
    if (f == "DPT") {
        return decode_dpt(sentence, state);
    }
    if (f == "DBT") {
        return decode_dbt(sentence, state);
    }
    if (f == "MTW") {
        return decode_mtw(sentence, state);
    }
    if (f == "MWV") {
        return decode_mwv(sentence, state);
    }
    if (f == "MWD") {
        return decode_mwd(sentence, state);
    }
    if (f == "RSA") {
        return decode_rsa(sentence, state);
    }
    if (f == "RMB") {
        return decode_rmb(sentence, state);
    }
    if (f == "APB" || f == "XTE") {
        return decode_nothing(sentence, state);
    }
    if (f == "RPM") {
        return decode_rpm(sentence, state);
    }
    if (f == "XDR") {
        return decode_xdr(sentence, state);
    }
    return false;
}

bool apply_sentence(std::string_view line, model::VesselState& state) {
    const auto parsed = parse_sentence(line);
    return parsed && apply_sentence(*parsed, state);
}

}  // namespace nmeasim::core::nmea0183
