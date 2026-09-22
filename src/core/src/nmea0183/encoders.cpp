#include <nmeasim/core/nmea0183/encoders.hpp>
#include <nmeasim/core/nmea0183/fields.hpp>
#include <nmeasim/core/nmea0183/sentence_builder.hpp>
#include <nmeasim/core/units.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

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

}  // namespace nmeasim::core::nmea0183
