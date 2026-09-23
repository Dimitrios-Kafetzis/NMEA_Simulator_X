#pragma once

#include <nmeasim/core/model/vessel_state.hpp>

#include <cstdint>
#include <string>

/// ViewSync UDP packets as Google Earth and Liquid Galaxy exchange them, so that the camera
/// follows the simulated vessel.
namespace nmeasim::core::viewsync {

struct ViewSyncOptions {
    /// Height of the camera above the vessel's altitude, metres.
    double camera_altitude_m{500.0};
    /// Camera tilt, degrees from straight down; 0 looks down, 90 looks at the horizon.
    double tilt_deg{60.0};
    double roll_deg{0.0};
    /// Planet name; empty for Earth, otherwise `sky`, `mars` or `moon`.
    std::string planet;
};

/// Seconds from 0001-01-01T00:00:00Z to the Unix epoch, the offset ViewSync times use.
inline constexpr std::int64_t kSecondsBeforeUnixEpoch{62135596800};

/// One packet: `counter,latitude,longitude,altitude,heading,tilt,roll,start,end,planet`
/// without line terminator. The camera looks along the vessel's true heading from the
/// configured height; both times are the simulated clock in seconds since year 1.
[[nodiscard]] std::string encode_packet(const model::VesselState& state,
                                        const ViewSyncOptions& options, std::uint32_t counter);

}  // namespace nmeasim::core::viewsync
