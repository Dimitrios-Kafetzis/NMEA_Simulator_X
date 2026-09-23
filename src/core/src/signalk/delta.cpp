#include <nmeasim/core/geo/route.hpp>
#include <nmeasim/core/signalk/delta.hpp>
#include <nmeasim/core/time/iso8601.hpp>
#include <nmeasim/core/units.hpp>
#include <nmeasim/core/version.hpp>

#include <cctype>
#include <cmath>
#include <format>
#include <numbers>

namespace nmeasim::core::signalk {

namespace {

double radians(double degrees) noexcept {
    return degrees * std::numbers::pi / 180.0;
}

/// An angle in [0, 360) degrees as radians in (-pi, pi], positive to starboard.
double signed_radians(double degrees) noexcept {
    double angle = std::fmod(degrees, 360.0);
    if (angle > 180.0) {
        angle -= 360.0;
    } else if (angle <= -180.0) {
        angle += 360.0;
    }
    return radians(angle);
}

std::string position_json(geo::Position position) {
    return "{\"longitude\":" + json_number(position.longitude_deg) +
           ",\"latitude\":" + json_number(position.latitude_deg) + "}";
}

void add(std::vector<PathValue>& values, std::string path, std::string json) {
    values.push_back({std::move(path), std::move(json)});
}

void add_number(std::vector<PathValue>& values, std::string path, double value) {
    add(values, std::move(path), json_number(value));
}

}  // namespace

std::string json_string(std::string_view text) {
    std::string result = "\"";
    for (const char c : text) {
        switch (c) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    result += std::format("\\u{:04x}", static_cast<unsigned char>(c));
                } else {
                    result += c;
                }
        }
    }
    return result + "\"";
}

std::string json_number(double value) {
    if (!std::isfinite(value)) {
        return "null";
    }
    // Round to a fixed number of decimals so that noise below the instruments' resolution
    // does not produce twenty-digit values, then trim trailing zeros.
    std::string text = std::format("{:.7f}", value);
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (text.back() == '.') {
        text.pop_back();
    }
    if (text == "-0") {
        text = "0";
    }
    return text;
}

std::string engine_id(std::string_view label, std::size_t index) {
    std::string lowered;
    for (const char c : label) {
        if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
            lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    const auto engine = lowered.find("engine");
    if (engine != std::string::npos) {
        lowered.erase(engine, 6);
    }
    return lowered.empty() ? "engine" + std::to_string(index + 1) : lowered;
}

std::string default_context(const model::VesselState& state) {
    return "vessels.urn:mrn:imo:mmsi:" + std::to_string(state.ais.mmsi);
}

std::string effective_context(const SignalKOptions& options, const model::VesselState& state) {
    return options.context.empty() ? default_context(state) : options.context;
}

std::vector<PathValue> path_values(const model::VesselState& state) {
    std::vector<PathValue> values;
    const auto& navigation = state.navigation;
    const auto& gnss = state.gnss;

    add(values, "navigation.datetime", json_string(time::format_iso8601(state.time_utc)));
    if (gnss.has_fix) {
        add(values, "navigation.position",
            "{\"longitude\":" + json_number(navigation.position.longitude_deg) +
                ",\"latitude\":" + json_number(navigation.position.latitude_deg) +
                ",\"altitude\":" + json_number(navigation.altitude_m) + "}");
        add_number(values, "navigation.courseOverGroundTrue",
                   radians(navigation.course_over_ground_deg));
        add_number(values, "navigation.courseOverGroundMagnetic",
                   radians(navigation.course_over_ground_magnetic_deg()));
        add_number(values, "navigation.speedOverGround",
                   units::knots_to_mps(navigation.speed_over_ground_kn));
    }
    add_number(values, "navigation.headingTrue", radians(navigation.heading_true_deg));
    add_number(values, "navigation.headingMagnetic", radians(navigation.heading_magnetic_deg()));
    add_number(values, "navigation.magneticVariation", radians(navigation.magnetic_variation_deg));
    add_number(values, "navigation.magneticDeviation", radians(navigation.magnetic_deviation_deg));
    add_number(values, "navigation.speedThroughWater",
               units::knots_to_mps(navigation.speed_through_water_kn));
    add_number(values, "navigation.rateOfTurn",
               radians(navigation.rate_of_turn_deg_per_min) / 60.0);
    add(values, "navigation.gnss.type", json_string("GPS"));
    add(values, "navigation.gnss.methodQuality",
        json_string(!gnss.has_fix                                     ? "no GPS"
                    : gnss.quality == model::FixQuality::Differential ? "DGNSS fix"
                                                                      : "GNSS Fix"));
    add(values, "navigation.gnss.satellites",
        std::to_string(gnss.has_fix ? gnss.satellites_in_use : 0));
    if (gnss.has_fix) {
        add_number(values, "navigation.gnss.horizontalDilution", gnss.hdop);
        add_number(values, "navigation.gnss.positionDilution", gnss.pdop);
        add_number(values, "navigation.gnss.antennaAltitude", navigation.altitude_m);
        add_number(values, "navigation.gnss.geoidalSeparation", gnss.geoid_separation_m);
    }
    if (state.destination) {
        const auto& destination = *state.destination;
        const auto leg =
            geo::solve_leg(destination.origin, destination.position, navigation.position);
        add(values, "navigation.courseRhumbline.nextPoint.position",
            position_json(destination.position));
        add_number(values, "navigation.courseRhumbline.nextPoint.bearingTrue",
                   radians(leg.bearing_deg));
        add_number(values, "navigation.courseRhumbline.nextPoint.distance", leg.distance_m);
        add_number(values, "navigation.courseRhumbline.nextPoint.velocityMadeGood",
                   units::knots_to_mps(navigation.speed_over_ground_kn) *
                       std::cos(radians(navigation.course_over_ground_deg - leg.bearing_deg)));
        add(values, "navigation.courseRhumbline.previousPoint.position",
            position_json(destination.origin));
        add_number(values, "navigation.courseRhumbline.bearingTrackTrue",
                   radians(leg.leg_bearing_deg));
        add_number(values, "navigation.courseRhumbline.crossTrackError", leg.cross_track_m);
    }

    const auto& water = state.water;
    add_number(values, "environment.depth.belowTransducer", water.depth_below_transducer_m);
    if (water.transducer_offset_m >= 0.0) {
        add_number(values, "environment.depth.surfaceToTransducer", water.transducer_offset_m);
        add_number(values, "environment.depth.belowSurface",
                   water.depth_below_transducer_m + water.transducer_offset_m);
    } else {
        add_number(values, "environment.depth.transducerToKeel", -water.transducer_offset_m);
        add_number(values, "environment.depth.belowKeel",
                   water.depth_below_transducer_m + water.transducer_offset_m);
    }
    add_number(values, "environment.water.temperature",
               units::celsius_to_kelvin(water.temperature_c));
    const auto& wind = state.wind;
    add_number(values, "environment.wind.speedTrue", units::knots_to_mps(wind.true_speed_kn));
    add_number(values, "environment.wind.directionTrue", radians(wind.true_direction_deg));
    add_number(values, "environment.wind.angleTrueWater",
               signed_radians(wind.true_angle_relative_deg(navigation.heading_true_deg)));
    add_number(values, "environment.wind.speedApparent",
               units::knots_to_mps(wind.apparent_speed_kn));
    add_number(values, "environment.wind.angleApparent", signed_radians(wind.apparent_angle_deg));

    add_number(values, "steering.rudderAngle", radians(state.steering.rudder_angle_deg));

    for (std::size_t index = 0; index < state.engines.size(); ++index) {
        const auto& engine = state.engines[index];
        const std::string prefix = "propulsion." + engine_id(engine.label, index) + ".";
        add_number(values, prefix + "revolutions",
                   engine.running ? engine.revolutions_rpm / 60.0 : 0.0);
        add_number(values, prefix + "temperature",
                   units::celsius_to_kelvin(engine.coolant_temperature_c));
        add(values, prefix + "state", json_string(engine.running ? "started" : "stopped"));
    }
    return values;
}

std::string encode_delta(const model::VesselState& state, const SignalKOptions& options,
                         const std::function<bool(std::string_view)>& admit) {
    std::string values;
    for (const auto& value : path_values(state)) {
        if (admit && !admit(value.path)) {
            continue;
        }
        if (!values.empty()) {
            values += ',';
        }
        values += "{\"path\":" + json_string(value.path) + ",\"value\":" + value.json_value + "}";
    }
    return "{\"context\":" + json_string(effective_context(options, state)) +
           ",\"updates\":[{\"source\":{\"label\":" + json_string(options.source_label) +
           ",\"type\":\"simulator\"},\"timestamp\":" +
           json_string(time::format_iso8601(state.time_utc)) + ",\"values\":[" + values + "]}]}";
}

std::string encode_hello(const SignalKOptions& options, const model::VesselState& state,
                         std::chrono::system_clock::time_point now) {
    return "{\"name\":" + json_string(kProjectName) + ",\"version\":" + json_string(kVersion) +
           ",\"self\":" + json_string(effective_context(options, state)) +
           ",\"roles\":[\"master\",\"main\"],\"timestamp\":" +
           json_string(time::format_iso8601(now)) + "}";
}

}  // namespace nmeasim::core::signalk
