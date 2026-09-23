#include <nmeasim/core/ais/messages.hpp>
#include <nmeasim/core/geo/route.hpp>
#include <nmeasim/core/nmea0183/encoders.hpp>
#include <nmeasim/core/nmea0183/fields.hpp>
#include <nmeasim/core/nmea0183/sentence_builder.hpp>
#include <nmeasim/core/units.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numbers>
#include <string>

namespace nmeasim::core::nmea0183 {

namespace {

using model::VesselState;

/// Pseudo-random noise numbers reported for the simulated GPS constellation.
constexpr std::array<int, 12> kSatellitePrns{2, 5, 7, 9, 12, 15, 19, 21, 24, 25, 29, 30};
constexpr int kMaxSatellites{static_cast<int>(kSatellitePrns.size())};
constexpr int kSatellitesPerGsvSentence{4};

int clamp_satellites(int count) {
    return std::clamp(count, 0, kMaxSatellites);
}

/// Synthetic but stable elevation, azimuth and signal strength per satellite.
struct SatelliteView {
    int elevation_deg;
    int azimuth_deg;
    int snr_db;
};

SatelliteView satellite_view(int prn) {
    return {15 + (prn * 7) % 70, (prn * 37) % 360, 30 + prn % 15};
}

SentenceBuilder& add_position(SentenceBuilder& builder, const EncoderContext& context) {
    const auto& navigation = context.state.navigation;
    if (!context.state.gnss.has_fix) {
        return builder.empty(4);
    }
    const auto lat =
        format_latitude(navigation.position.latitude_deg, context.options.position_decimals);
    const auto lon =
        format_longitude(navigation.position.longitude_deg, context.options.position_decimals);
    return builder.field(lat.value).field(lat.hemisphere).field(lon.value).field(lon.hemisphere);
}

double degrees_to_radians(double degrees) {
    return degrees * std::numbers::pi / 180.0;
}

/// Values shared by the autopilot sentences for the current destination.
struct AutopilotView {
    std::string name;
    geo::LegSolution leg;
    /// Cross-track error magnitude in nautical miles and the side to steer to.
    double cross_track_nm;
    char steer;
    bool arrived;
    bool perpendicular_passed;
};

AutopilotView autopilot_view(const model::VesselState& state) {
    const auto& destination = *state.destination;
    AutopilotView view;
    view.name = sanitize_waypoint_name(destination.name);
    view.leg = geo::solve_leg(destination.origin, destination.position, state.navigation.position);
    view.cross_track_nm = std::fabs(view.leg.cross_track_m) / units::kMetresPerNauticalMile;
    // A vessel to the right of the leg steers left to regain it.
    view.steer = view.leg.cross_track_m > 0.0 ? 'L' : 'R';
    view.arrived = view.leg.distance_m <= destination.arrival_radius_m;
    view.perpendicular_passed = view.leg.along_track_m >= view.leg.leg_length_m;
    return view;
}

char valid(bool flag) noexcept {
    return flag ? 'A' : 'V';
}

/// Sequential message id of a multi-sentence AIS message, derived from the clock so that the
/// fragments of one message share it and consecutive messages differ.
int ais_sequence(const model::VesselState& state) {
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(state.time_utc.time_since_epoch()).count();
    return static_cast<int>(((seconds % 10) + 10) % 10);
}

std::vector<std::string> encode_ais(const EncoderContext& context, std::string_view formatter,
                                    bool static_data) {
    const auto packer = static_data ? ais::pack_static_data(context.state)
                                    : ais::pack_position_report(context.state);
    return ais::frame_payload(context.talker, formatter, ais::armor(packer.bits()),
                              ais_sequence(context.state));
}

/// The engine number sent in RPM: engines are numbered from 1 in profile order.
int engine_number(std::size_t index) noexcept {
    return static_cast<int>(index) + 1;
}

/// The transducer identifier sent in XDR, following the ENGINE#n convention of Signal K and
/// common gateways, numbered from 0 in profile order.
std::string transducer_id(std::size_t index) {
    return "ENGINE#" + std::to_string(index);
}

}  // namespace

char mode_indicator(const model::GnssFix& fix) noexcept {
    if (!fix.has_fix) {
        return 'N';
    }
    return fix.quality == model::FixQuality::Differential ? 'D' : 'A';
}

char status_indicator(const model::GnssFix& fix) noexcept {
    return fix.has_fix ? 'A' : 'V';
}

// ---------------------------------------------------------------------------------------------
// GNSS

std::vector<std::string> encode_rmc(const EncoderContext& context) {
    const auto& state = context.state;
    const auto& navigation = state.navigation;
    SentenceBuilder builder(context.talker, "RMC");
    builder.field(format_time(state.time_utc)).field(status_indicator(state.gnss));
    add_position(builder, context);
    if (state.gnss.has_fix) {
        builder.field(navigation.speed_over_ground_kn, 1)
            .field(navigation.course_over_ground_deg, 1);
    } else {
        builder.empty(2);
    }
    builder.field(format_date(state.time_utc))
        .field(std::fabs(navigation.magnetic_variation_deg), 1)
        .field(east_west(navigation.magnetic_variation_deg))
        .field(mode_indicator(state.gnss));
    return {builder.build()};
}

std::vector<std::string> encode_gga(const EncoderContext& context) {
    const auto& state = context.state;
    SentenceBuilder builder(context.talker, "GGA");
    builder.field(format_time(state.time_utc));
    add_position(builder, context);
    if (state.gnss.has_fix) {
        builder.field(static_cast<int>(state.gnss.quality))
            .field(clamp_satellites(state.gnss.satellites_in_use), 2)
            .field(state.gnss.hdop, 1)
            .field(state.navigation.altitude_m, 1)
            .field('M')
            .field(state.gnss.geoid_separation_m, 1)
            .field('M');
    } else {
        builder.field(0).field(0, 2).empty().empty().field('M').empty().field('M');
    }
    builder.empty(2);  // age of differential data, differential station id
    return {builder.build()};
}

std::vector<std::string> encode_gll(const EncoderContext& context) {
    const auto& state = context.state;
    SentenceBuilder builder(context.talker, "GLL");
    add_position(builder, context);
    builder.field(format_time(state.time_utc))
        .field(status_indicator(state.gnss))
        .field(mode_indicator(state.gnss));
    return {builder.build()};
}

std::vector<std::string> encode_gsa(const EncoderContext& context) {
    const auto& fix = context.state.gnss;
    SentenceBuilder builder(context.talker, "GSA");
    builder.field('A').field(fix.has_fix ? 3 : 1);
    const int in_use = fix.has_fix ? clamp_satellites(fix.satellites_in_use) : 0;
    for (int i = 0; i < kMaxSatellites; ++i) {
        if (i < in_use) {
            builder.field(kSatellitePrns[static_cast<std::size_t>(i)], 2);
        } else {
            builder.empty();
        }
    }
    if (fix.has_fix) {
        builder.field(fix.pdop, 1).field(fix.hdop, 1).field(fix.vdop, 1);
    } else {
        builder.empty(3);
    }
    return {builder.build()};
}

std::vector<std::string> encode_gsv(const EncoderContext& context) {
    const auto& fix = context.state.gnss;
    const int in_view = fix.has_fix ? std::max(clamp_satellites(fix.satellites_in_view),
                                               clamp_satellites(fix.satellites_in_use))
                                    : 0;
    if (in_view == 0) {
        SentenceBuilder builder(context.talker, "GSV");
        builder.field(1).field(1).field(0, 2);
        return {builder.build()};
    }
    const int total = (in_view + kSatellitesPerGsvSentence - 1) / kSatellitesPerGsvSentence;
    std::vector<std::string> sentences;
    sentences.reserve(static_cast<std::size_t>(total));
    for (int index = 0; index < total; ++index) {
        SentenceBuilder builder(context.talker, "GSV");
        builder.field(total).field(index + 1).field(in_view, 2);
        const int first = index * kSatellitesPerGsvSentence;
        const int last = std::min(first + kSatellitesPerGsvSentence, in_view);
        for (int i = first; i < last; ++i) {
            const int prn = kSatellitePrns[static_cast<std::size_t>(i)];
            const auto view = satellite_view(prn);
            builder.field(prn, 2)
                .field(view.elevation_deg, 2)
                .field(view.azimuth_deg, 3)
                .field(view.snr_db, 2);
        }
        sentences.push_back(builder.build());
    }
    return sentences;
}

std::vector<std::string> encode_vtg(const EncoderContext& context) {
    const auto& state = context.state;
    const auto& navigation = state.navigation;
    SentenceBuilder builder(context.talker, "VTG");
    if (state.gnss.has_fix) {
        builder.field(navigation.course_over_ground_deg, 1)
            .field('T')
            .field(navigation.course_over_ground_magnetic_deg(), 1)
            .field('M')
            .field(navigation.speed_over_ground_kn, 1)
            .field('N')
            .field(units::knots_to_kmh(navigation.speed_over_ground_kn), 1)
            .field('K');
    } else {
        builder.empty().field('T').empty().field('M').empty().field('N').empty().field('K');
    }
    builder.field(mode_indicator(state.gnss));
    return {builder.build()};
}

std::vector<std::string> encode_zda(const EncoderContext& context) {
    const auto& state = context.state;
    const auto parts = date_parts(state.time_utc);
    SentenceBuilder builder(context.talker, "ZDA");
    builder.field(format_time(state.time_utc))
        .field(parts.day, 2)
        .field(parts.month, 2)
        .field(parts.year, 4)
        .field(0, 2)
        .field(0, 2);
    return {builder.build()};
}

// ---------------------------------------------------------------------------------------------
// Heading and speed

std::vector<std::string> encode_hdg(const EncoderContext& context) {
    const auto& navigation = context.state.navigation;
    SentenceBuilder builder(context.talker, "HDG");
    builder.field(navigation.heading_magnetic_deg(), 1)
        .field(std::fabs(navigation.magnetic_deviation_deg), 1)
        .field(east_west(navigation.magnetic_deviation_deg))
        .field(std::fabs(navigation.magnetic_variation_deg), 1)
        .field(east_west(navigation.magnetic_variation_deg));
    return {builder.build()};
}

std::vector<std::string> encode_hdm(const EncoderContext& context) {
    SentenceBuilder builder(context.talker, "HDM");
    builder.field(context.state.navigation.heading_magnetic_deg(), 1).field('M');
    return {builder.build()};
}

std::vector<std::string> encode_hdt(const EncoderContext& context) {
    SentenceBuilder builder(context.talker, "HDT");
    builder.field(context.state.navigation.heading_true_deg, 1).field('T');
    return {builder.build()};
}

std::vector<std::string> encode_vhw(const EncoderContext& context) {
    const auto& navigation = context.state.navigation;
    SentenceBuilder builder(context.talker, "VHW");
    builder.field(navigation.heading_true_deg, 1)
        .field('T')
        .field(navigation.heading_magnetic_deg(), 1)
        .field('M')
        .field(navigation.speed_through_water_kn, 1)
        .field('N')
        .field(units::knots_to_kmh(navigation.speed_through_water_kn), 1)
        .field('K');
    return {builder.build()};
}

std::vector<std::string> encode_vbw(const EncoderContext& context) {
    const auto& navigation = context.state.navigation;
    // Ground speed is resolved onto the vessel's axes using the drift angle between course
    // over ground and heading. Water speed is assumed to have no leeway.
    const double drift =
        degrees_to_radians(navigation.course_over_ground_deg - navigation.heading_true_deg);
    const double ground_longitudinal = navigation.speed_over_ground_kn * std::cos(drift);
    const double ground_transverse = navigation.speed_over_ground_kn * std::sin(drift);
    SentenceBuilder builder(context.talker, "VBW");
    builder.field(navigation.speed_through_water_kn, 1)
        .field(0.0, 1)
        .field('A')
        .field(ground_longitudinal, 1)
        .field(ground_transverse, 1)
        .field('A')
        .field(0.0, 1)
        .field('A')
        .field(0.0, 1)
        .field('A');
    return {builder.build()};
}

std::vector<std::string> encode_rot(const EncoderContext& context) {
    SentenceBuilder builder(context.talker, "ROT");
    builder.field(context.state.navigation.rate_of_turn_deg_per_min, 1).field('A');
    return {builder.build()};
}

// ---------------------------------------------------------------------------------------------
// Depth and water

std::vector<std::string> encode_dpt(const EncoderContext& context) {
    const auto& water = context.state.water;
    SentenceBuilder builder(context.talker, "DPT");
    builder.field(water.depth_below_transducer_m, 1).field(water.transducer_offset_m, 1).empty();
    return {builder.build()};
}

std::vector<std::string> encode_dbt(const EncoderContext& context) {
    const double depth = context.state.water.depth_below_transducer_m;
    SentenceBuilder builder(context.talker, "DBT");
    builder.field(units::metres_to_feet(depth), 1)
        .field('f')
        .field(depth, 1)
        .field('M')
        .field(units::metres_to_fathoms(depth), 1)
        .field('F');
    return {builder.build()};
}

std::vector<std::string> encode_mtw(const EncoderContext& context) {
    SentenceBuilder builder(context.talker, "MTW");
    builder.field(context.state.water.temperature_c, 1).field('C');
    return {builder.build()};
}

// ---------------------------------------------------------------------------------------------
// Wind

std::vector<std::string> encode_mwv_apparent(const EncoderContext& context) {
    const auto& wind = context.state.wind;
    SentenceBuilder builder(context.talker, "MWV");
    builder.field(wind.apparent_angle_deg, 1)
        .field('R')
        .field(wind.apparent_speed_kn, 1)
        .field('N')
        .field('A');
    return {builder.build()};
}

std::vector<std::string> encode_mwv_true(const EncoderContext& context) {
    const auto& state = context.state;
    SentenceBuilder builder(context.talker, "MWV");
    builder.field(state.wind.true_angle_relative_deg(state.navigation.heading_true_deg), 1)
        .field('T')
        .field(state.wind.true_speed_kn, 1)
        .field('N')
        .field('A');
    return {builder.build()};
}

std::vector<std::string> encode_mwd(const EncoderContext& context) {
    const auto& state = context.state;
    const double magnetic_direction = geo::normalize_bearing(
        state.wind.true_direction_deg - state.navigation.magnetic_variation_deg);
    SentenceBuilder builder(context.talker, "MWD");
    builder.field(state.wind.true_direction_deg, 1)
        .field('T')
        .field(magnetic_direction, 1)
        .field('M')
        .field(state.wind.true_speed_kn, 1)
        .field('N')
        .field(units::knots_to_mps(state.wind.true_speed_kn), 1)
        .field('M');
    return {builder.build()};
}

// ---------------------------------------------------------------------------------------------
// Steering

std::vector<std::string> encode_rsa(const EncoderContext& context) {
    SentenceBuilder builder(context.talker, "RSA");
    builder.field(context.state.steering.rudder_angle_deg, 1).field('A').empty().field('V');
    return {builder.build()};
}

// ---------------------------------------------------------------------------------------------
// Autopilot

std::string sanitize_waypoint_name(std::string_view name) {
    std::string result;
    for (const char c : name) {
        const bool printable = c > ' ' && c <= '~';
        const bool reserved =
            c == ',' || c == '*' || c == '$' || c == '!' || c == '\\' || c == '^' || c == '~';
        if (printable && !reserved) {
            result += c;
        }
        if (result.size() == model::kMaxWaypointNameLength) {
            break;
        }
    }
    return result.empty() ? "WPT" : result;
}

std::vector<std::string> encode_apb(const EncoderContext& context) {
    const auto& state = context.state;
    if (!state.destination) {
        return {};
    }
    const auto view = autopilot_view(state);
    SentenceBuilder builder(context.talker, "APB");
    builder.field('A')
        .field('A')
        .field(view.cross_track_nm, 2)
        .field(view.steer)
        .field('N')
        .field(valid(view.arrived))
        .field(valid(view.perpendicular_passed))
        .field(view.leg.leg_bearing_deg, 1)
        .field('T')
        .field(view.name)
        .field(view.leg.bearing_deg, 1)
        .field('T')
        .field(view.leg.bearing_deg, 1)
        .field('T')
        .field(mode_indicator(state.gnss));
    return {builder.build()};
}

std::vector<std::string> encode_rmb(const EncoderContext& context) {
    const auto& state = context.state;
    if (!state.destination) {
        return {};
    }
    const auto view = autopilot_view(state);
    const auto& navigation = state.navigation;
    const auto lat = format_latitude(state.destination->position.latitude_deg,
                                     context.options.position_decimals);
    const auto lon = format_longitude(state.destination->position.longitude_deg,
                                      context.options.position_decimals);
    const double range_nm = std::min(view.leg.distance_m / units::kMetresPerNauticalMile, 999.9);
    const double closing_kn =
        navigation.speed_over_ground_kn *
        std::cos(degrees_to_radians(navigation.course_over_ground_deg - view.leg.bearing_deg));
    SentenceBuilder builder(context.talker, "RMB");
    builder.field('A')
        .field(view.cross_track_nm, 2)
        .field(view.steer)
        .empty()
        .field(view.name)
        .field(lat.value)
        .field(lat.hemisphere)
        .field(lon.value)
        .field(lon.hemisphere)
        .field(range_nm, 1)
        .field(view.leg.bearing_deg, 1)
        .field(closing_kn, 1)
        .field(valid(view.arrived))
        .field(mode_indicator(state.gnss));
    return {builder.build()};
}

std::vector<std::string> encode_xte(const EncoderContext& context) {
    const auto& state = context.state;
    if (!state.destination) {
        return {};
    }
    const auto view = autopilot_view(state);
    SentenceBuilder builder(context.talker, "XTE");
    builder.field('A')
        .field('A')
        .field(view.cross_track_nm, 2)
        .field(view.steer)
        .field('N')
        .field(mode_indicator(state.gnss));
    return {builder.build()};
}

// ---------------------------------------------------------------------------------------------
// Propulsion

std::vector<std::string> encode_rpm(const EncoderContext& context) {
    std::vector<std::string> sentences;
    const auto& engines = context.state.engines;
    sentences.reserve(engines.size());
    for (std::size_t index = 0; index < engines.size(); ++index) {
        const auto& engine = engines[index];
        SentenceBuilder builder(context.talker, "RPM");
        builder.field('E')
            .field(engine_number(index))
            .field(engine.running ? engine.revolutions_rpm : 0.0, 1)
            .empty()
            .field('A');
        sentences.push_back(builder.build());
    }
    return sentences;
}

std::vector<std::string> encode_xdr(const EncoderContext& context) {
    std::vector<std::string> sentences;
    const auto& engines = context.state.engines;
    sentences.reserve(engines.size());
    for (std::size_t index = 0; index < engines.size(); ++index) {
        const auto& engine = engines[index];
        SentenceBuilder builder(context.talker, "XDR");
        builder.field('C')
            .field(engine.coolant_temperature_c, 1)
            .field('C')
            .field(transducer_id(index))
            .field('T')
            .field(engine.running ? engine.revolutions_rpm : 0.0, 1)
            .field('R')
            .field(transducer_id(index));
        sentences.push_back(builder.build());
    }
    return sentences;
}

// ---------------------------------------------------------------------------------------------
// AIS

std::vector<std::string> encode_vdo_position(const EncoderContext& context) {
    return encode_ais(context, "VDO", false);
}

std::vector<std::string> encode_vdo_static(const EncoderContext& context) {
    return encode_ais(context, "VDO", true);
}

std::vector<std::string> encode_vdm_position(const EncoderContext& context) {
    return encode_ais(context, "VDM", false);
}

std::vector<std::string> encode_vdm_static(const EncoderContext& context) {
    return encode_ais(context, "VDM", true);
}

}  // namespace nmeasim::core::nmea0183
