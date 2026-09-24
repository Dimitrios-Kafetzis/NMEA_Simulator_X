// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Formatting of ViewSync camera packets from the vessel state.

#include <nmeasim/core/viewsync/viewsync.hpp>

#include <chrono>
#include <format>
#include <string>
#include <string_view>

namespace nmeasim::core::viewsync {

std::string sanitize_planet(std::string_view planet) {
    std::string result;
    for (const char c : planet) {
        if (c >= ' ' && c <= '~' && c != ',') {
            result += c;
        }
    }
    return result;
}

std::string encode_packet(const model::VesselState& state, const ViewSyncOptions& options,
                          std::uint32_t counter) {
    const auto& navigation = state.navigation;
    const auto unix_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(state.time_utc.time_since_epoch()).count();
    // Google Earth counts time from the start of year 1, not from the Unix epoch.
    const std::int64_t time = static_cast<std::int64_t>(unix_seconds) + kSecondsBeforeUnixEpoch;
    return std::format("{},{:.7f},{:.7f},{:.2f},{:.2f},{:.2f},{:.2f},{},{},{}", counter,
                       navigation.position.latitude_deg, navigation.position.longitude_deg,
                       navigation.altitude_m + options.camera_altitude_m,
                       navigation.heading_true_deg, options.tilt_deg, options.roll_deg, time, time,
                       sanitize_planet(options.planet));
}

}  // namespace nmeasim::core::viewsync
