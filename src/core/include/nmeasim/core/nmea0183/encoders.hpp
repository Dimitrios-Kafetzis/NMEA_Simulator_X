#pragma once

#include <nmeasim/core/model/vessel_state.hpp>

#include <string>
#include <string_view>
#include <vector>

/// Encoders that turn a vessel state into NMEA 0183 sentences.
///
/// Each encoder returns every sentence it produces for one emission: most return exactly one,
/// GSV returns as many as needed to list the satellites in view. Encoders never throw and
/// never emit a sentence over the length limit for the given options; the registry lowers the
/// position precision if a sentence would otherwise be too long.
namespace nmeasim::core::nmea0183 {

struct EncoderOptions {
    /// Fractional minute digits in latitude and longitude fields. 4 digits resolve 0.19 m.
    int position_decimals{4};
};

struct EncoderContext {
    const model::VesselState& state;
    /// Two-character talker identifier already resolved from configuration.
    std::string_view talker;
    EncoderOptions options{};
};

using Encoder = std::vector<std::string> (*)(const EncoderContext&);

// GNSS
std::vector<std::string> encode_rmc(const EncoderContext& context);
std::vector<std::string> encode_gga(const EncoderContext& context);
std::vector<std::string> encode_gll(const EncoderContext& context);
std::vector<std::string> encode_gsa(const EncoderContext& context);
std::vector<std::string> encode_gsv(const EncoderContext& context);
std::vector<std::string> encode_vtg(const EncoderContext& context);
std::vector<std::string> encode_zda(const EncoderContext& context);

// Heading and speed
std::vector<std::string> encode_hdg(const EncoderContext& context);
std::vector<std::string> encode_hdm(const EncoderContext& context);
std::vector<std::string> encode_hdt(const EncoderContext& context);
std::vector<std::string> encode_vhw(const EncoderContext& context);
std::vector<std::string> encode_vbw(const EncoderContext& context);
std::vector<std::string> encode_rot(const EncoderContext& context);

// Depth and water
std::vector<std::string> encode_dpt(const EncoderContext& context);
std::vector<std::string> encode_dbt(const EncoderContext& context);
std::vector<std::string> encode_mtw(const EncoderContext& context);

// Wind
std::vector<std::string> encode_mwv_apparent(const EncoderContext& context);
std::vector<std::string> encode_mwv_true(const EncoderContext& context);
std::vector<std::string> encode_mwd(const EncoderContext& context);

// Steering
std::vector<std::string> encode_rsa(const EncoderContext& context);

/// Mode indicator letter used by RMC, GLL and VTG: 'A' autonomous, 'D' differential,
/// 'N' no fix.
[[nodiscard]] char mode_indicator(const model::GnssFix& fix) noexcept;

/// Status letter used by RMC and GLL: 'A' valid, 'V' invalid.
[[nodiscard]] char status_indicator(const model::GnssFix& fix) noexcept;

}  // namespace nmeasim::core::nmea0183
