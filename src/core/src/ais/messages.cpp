#include <nmeasim/core/ais/messages.hpp>
#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/sentence_builder.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace nmeasim::core::ais {

namespace {

/// Longest payload put in one sentence, chosen so that a two-fragment sentence such as
/// `!AIVDM,2,1,3,A,<payload>,0*hh` stays within 82 bytes.
constexpr std::size_t kMaxPayloadPerSentence{60};

std::uint32_t seconds_of_minute(std::chrono::system_clock::time_point time) {
    using namespace std::chrono;
    const auto since_midnight = time - floor<days>(time);
    return static_cast<std::uint32_t>(duration_cast<seconds>(since_midnight).count() % 60);
}

std::int32_t round_to(double value) {
    return static_cast<std::int32_t>(std::lround(value));
}

std::uint32_t clamp_unsigned(double value, std::uint32_t maximum) {
    return static_cast<std::uint32_t>(
        std::clamp(std::lround(value), 0L, static_cast<long>(maximum)));
}

}  // namespace

int rate_of_turn_code(double rate_deg_per_min) noexcept {
    const double code = 4.733 * std::sqrt(std::fabs(rate_deg_per_min));
    const int magnitude = std::min(static_cast<int>(std::lround(code)), 126);
    return rate_deg_per_min < 0.0 ? -magnitude : magnitude;
}

BitPacker pack_position_report(const model::VesselState& state) {
    const auto& ais = state.ais;
    const auto& navigation = state.navigation;
    const bool fix = state.gnss.has_fix;
    BitPacker packer;
    const int type = ais.position_report_type >= 1 && ais.position_report_type <= 3
                         ? ais.position_report_type
                         : 1;
    packer.append_unsigned(static_cast<std::uint32_t>(type), 6);
    packer.append_unsigned(0, 2);  // repeat indicator
    packer.append_unsigned(ais.mmsi, 30);
    packer.append_unsigned(static_cast<std::uint32_t>(std::clamp(ais.navigation_status, 0, 15)), 4);
    packer.append_signed(rate_of_turn_code(navigation.rate_of_turn_deg_per_min), 8);
    // Speed over ground in tenths of a knot; 1022 means 102.2 knots or more, 1023 unknown.
    packer.append_unsigned(
        fix ? std::min(clamp_unsigned(navigation.speed_over_ground_kn * 10.0, 1023U), 1022U)
            : 1023U,
        10);
    packer.append_bool(fix && state.gnss.quality == model::FixQuality::Differential);
    // Position in ten-thousandths of a minute; 181 degrees east and 91 degrees north mean
    // "not available".
    packer.append_signed(
        fix ? round_to(navigation.position.longitude_deg * 600000.0) : 181 * 600000, 28);
    packer.append_signed(fix ? round_to(navigation.position.latitude_deg * 600000.0) : 91 * 600000,
                         27);
    packer.append_unsigned(
        fix ? clamp_unsigned(navigation.course_over_ground_deg * 10.0, 3599U) : 3600U, 12);
    packer.append_unsigned(clamp_unsigned(navigation.heading_true_deg, 359U), 9);
    packer.append_unsigned(seconds_of_minute(state.time_utc), 6);
    packer.append_unsigned(0, 2);   // manoeuvre indicator: not available
    packer.append_unsigned(0, 3);   // spare
    packer.append_bool(false);      // RAIM not in use
    packer.append_unsigned(0, 19);  // radio status
    return packer;
}

BitPacker pack_static_data(const model::VesselState& state) {
    const auto& ais = state.ais;
    BitPacker packer;
    packer.append_unsigned(5, 6);
    packer.append_unsigned(0, 2);  // repeat indicator
    packer.append_unsigned(ais.mmsi, 30);
    packer.append_unsigned(0, 2);  // AIS version: ITU-R M.1371-1
    packer.append_unsigned(ais.imo_number, 30);
    packer.append_text(ais.call_sign, 7);
    packer.append_text(ais.name, 20);
    packer.append_unsigned(static_cast<std::uint32_t>(std::clamp(ais.ship_type, 0, 255)), 8);
    packer.append_unsigned(clamp_unsigned(ais.dimension_to_bow_m, 511U), 9);
    packer.append_unsigned(clamp_unsigned(ais.dimension_to_stern_m, 511U), 9);
    packer.append_unsigned(clamp_unsigned(ais.dimension_to_port_m, 63U), 6);
    packer.append_unsigned(clamp_unsigned(ais.dimension_to_starboard_m, 63U), 6);
    packer.append_unsigned(1, 4);   // position fix device: GPS
    packer.append_unsigned(0, 4);   // ETA month: not available
    packer.append_unsigned(0, 5);   // ETA day
    packer.append_unsigned(24, 5);  // ETA hour: not available
    packer.append_unsigned(60, 6);  // ETA minute: not available
    packer.append_unsigned(clamp_unsigned(ais.draught_m * 10.0, 255U), 8);
    packer.append_text(ais.destination, 20);
    packer.append_bool(false);  // DTE: data terminal ready
    packer.append_bool(false);  // spare
    return packer;
}

std::vector<std::string> frame_payload(std::string_view talker, std::string_view formatter,
                                       const Payload& payload, int sequence) {
    const std::size_t total = std::max<std::size_t>(
        1, (payload.text.size() + kMaxPayloadPerSentence - 1) / kMaxPayloadPerSentence);
    std::vector<std::string> sentences;
    sentences.reserve(total);
    for (std::size_t index = 0; index < total; ++index) {
        const auto fragment = std::string_view{payload.text}.substr(index * kMaxPayloadPerSentence,
                                                                    kMaxPayloadPerSentence);
        const bool last = index + 1 == total;
        nmea0183::SentenceBuilder builder(talker, formatter, nmea0183::kEncapsulationDelimiter);
        builder.field(static_cast<int>(total)).field(static_cast<int>(index) + 1);
        if (total > 1) {
            builder.field(std::clamp(sequence, 0, 9));
        } else {
            builder.empty();
        }
        builder.field('A').field(fragment).field(last ? payload.fill_bits : 0);
        sentences.push_back(builder.build());
    }
    return sentences;
}

}  // namespace nmeasim::core::ais
