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

std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

bool all_digits(std::string_view text) noexcept {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

int digits_value(std::string_view text) noexcept {
    int value = 0;
    for (const char c : text) {
        value = value * 10 + (c - '0');
    }
    return value;
}

/// Applies a signed east/west quantity such as variation.
std::optional<double> signed_east_west(std::string_view value, std::string_view direction) {
    const auto magnitude = parse_number_field(value);
    if (!magnitude) {
        return std::nullopt;
    }
    return direction == "W" ? -*magnitude : *magnitude;
}

/// Sets `state.time_utc` from a time of day, keeping the date of the current state time.
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

template <typename T>
void assign(std::optional<T> value, T& target) {
    if (value) {
        target = *value;
    }
}

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
        return *speed / 1.15078;  // statute miles per hour
    }
    return *speed;
}

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

bool decode_gsv(const ParsedSentence& s, model::VesselState& state) {
    if (const auto in_view = parse_number_field(s.field(2))) {
        state.gnss.satellites_in_view = static_cast<int>(*in_view);
    }
    return true;
}

bool decode_vtg(const ParsedSentence& s, model::VesselState& state) {
    assign(parse_number_field(s.field(0)), state.navigation.course_over_ground_deg);
    assign(parse_number_field(s.field(4)), state.navigation.speed_over_ground_kn);
    return true;
}

bool decode_zda(const ParsedSentence& s, model::VesselState& state) {
    if (const auto time = sentence_time(s)) {
        apply_time(state, *time);
    }
    return true;
}

bool decode_hdt(const ParsedSentence& s, model::VesselState& state) {
    if (const auto heading = parse_number_field(s.field(0))) {
        state.navigation.heading_true_deg = geo::normalize_bearing(*heading);
    }
    return true;
}

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

bool decode_hdm(const ParsedSentence& s, model::VesselState& state) {
    if (const auto magnetic = parse_number_field(s.field(0))) {
        state.navigation.heading_true_deg =
            geo::normalize_bearing(*magnetic + state.navigation.magnetic_variation_deg +
                                   state.navigation.magnetic_deviation_deg);
    }
    return true;
}

bool decode_rot(const ParsedSentence& s, model::VesselState& state) {
    assign(parse_number_field(s.field(0)), state.navigation.rate_of_turn_deg_per_min);
    return true;
}

bool decode_vhw(const ParsedSentence& s, model::VesselState& state) {
    if (const auto heading = parse_number_field(s.field(0))) {
        state.navigation.heading_true_deg = geo::normalize_bearing(*heading);
    }
    assign(parse_number_field(s.field(4)), state.navigation.speed_through_water_kn);
    return true;
}

bool decode_vbw(const ParsedSentence& s, model::VesselState& state) {
    // Only the water speed is taken: the ground speed is resolved onto the vessel's axes
    // and rounded, so RMC and VTG carry it more precisely.
    if (s.field(2) != "V") {
        assign(parse_number_field(s.field(0)), state.navigation.speed_through_water_kn);
    }
    return true;
}

bool decode_dpt(const ParsedSentence& s, model::VesselState& state) {
    assign(parse_number_field(s.field(0)), state.water.depth_below_transducer_m);
    assign(parse_number_field(s.field(1)), state.water.transducer_offset_m);
    return true;
}

bool decode_dbt(const ParsedSentence& s, model::VesselState& state) {
    if (const auto metres = parse_number_field(s.field(2))) {
        state.water.depth_below_transducer_m = *metres;
    } else if (const auto feet = parse_number_field(s.field(0))) {
        state.water.depth_below_transducer_m = *feet / units::kFeetPerMetre;
    }
    return true;
}

bool decode_mtw(const ParsedSentence& s, model::VesselState& state) {
    assign(parse_number_field(s.field(0)), state.water.temperature_c);
    return true;
}

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

/// APB and XTE repeat what RMB carries without the destination position, so they are
/// recognised but leave the state alone.
bool decode_nothing(const ParsedSentence& /*s*/, model::VesselState& /*state*/) {
    return true;
}

bool decode_rmb(const ParsedSentence& s, model::VesselState& state) {
    if (s.field(0) != "A") {
        return true;
    }
    const auto latitude = parse_coordinate(s.field(5), s.field(6));
    const auto longitude = parse_coordinate(s.field(7), s.field(8));
    if (!latitude || !longitude) {
        return true;
    }
    const std::string name{s.field(4)};
    // The sentence does not carry the origin: a new destination starts its leg where the
    // vessel is, an update of the same destination keeps the leg.
    const bool same = state.destination && state.destination->name == name;
    model::Destination destination;
    destination.name = name.empty() ? "WPT" : name;
    destination.position = {*latitude, *longitude};
    destination.origin = same ? state.destination->origin : state.navigation.position;
    if (same) {
        destination.arrival_radius_m = state.destination->arrival_radius_m;
    }
    state.destination = destination;
    return true;
}

model::Engine& engine_at(model::VesselState& state, std::size_t index) {
    while (state.engines.size() <= index) {
        model::Engine engine;
        engine.label = "Engine " + std::to_string(state.engines.size() + 1);
        state.engines.push_back(engine);
    }
    return state.engines[index];
}

/// Index of the engine named by an XDR transducer id such as "ENGINE#1".
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
