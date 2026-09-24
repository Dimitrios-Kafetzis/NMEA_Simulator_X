// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// ViewSync camera packets that make Google Earth follow the simulated vessel.
///
/// `encode_packet` formats one packet from the vessel state; an output with the `viewsync`
/// encoding sends one per period over UDP. docs/reference/viewsync.md describes the layout.

#pragma once

#include <nmeasim/core/model/vessel_state.hpp>

#include <cstdint>
#include <string>
#include <string_view>

/// ViewSync output, part of the `nmeasim::core` library.
///
/// It formats the UDP packets that Google Earth and Liquid Galaxy exchange to synchronise
/// their camera, so that a Google Earth instance with ViewSync enabled follows the simulated
/// vessel. Sending them is the job of `nmeasim::io`.
///
/// @see https://github.com/LiquidGalaxy/liquid-galaxy/wiki/GoogleEarth_ViewSync
namespace nmeasim::core::viewsync {

/// Camera settings for the packets of one output.
struct ViewSyncOptions {
    /// Height of the camera above the vessel's altitude.
    double camera_altitude_m{500.0};
    /// Camera tilt in degrees from straight down: 0 looks down, 90 looks at the horizon.
    double tilt_deg{60.0};
    /// Camera roll in degrees.
    double roll_deg{0.0};
    /// Planet name: empty for Earth, otherwise `sky`, `mars` or `moon`. It is sent through
    /// sanitize_planet(), so a comma or a line break in it cannot corrupt the packet.
    std::string planet;
};

/// Seconds from 0001-01-01T00:00:00Z to the Unix epoch in the proleptic Gregorian calendar,
/// the origin of ViewSync times.
inline constexpr std::int64_t kSecondsBeforeUnixEpoch{62135596800};

/// Returns a planet name without the characters that would corrupt a packet.
///
/// Commas (the field separator), control characters such as CR and LF, DEL and bytes outside
/// ASCII are removed; every other character is kept, so the valid names `sky`, `mars` and
/// `moon` and any other name Google Earth may accept are sent unchanged.
///
/// @param planet The configured planet name, of any content.
/// @return The name with only printable ASCII characters other than the comma; empty when
///     none is left, which Google Earth takes as Earth.
[[nodiscard]] std::string sanitize_planet(std::string_view planet);

/// Encodes one packet: `counter,latitude,longitude,altitude,heading,tilt,roll,start,end,planet`.
///
/// Latitude and longitude are the vessel's position in degrees with seven decimals; the
/// altitude is the vessel's altitude plus `options.camera_altitude_m`; the heading is the
/// vessel's true heading, so the camera looks along it. Altitude, heading, tilt and roll have
/// two decimals. Start and end are both the simulated clock in whole seconds since
/// 0001-01-01T00:00:00Z. The planet is `options.planet` after sanitize_planet().
///
/// @param state Vessel state that places the camera.
/// @param options Camera settings.
/// @param counter Packet counter; the caller increases it by one per packet on the output.
/// @return The packet without line terminator.
/// @see https://github.com/LiquidGalaxy/liquid-galaxy/wiki/GoogleEarth_ViewSync
[[nodiscard]] std::string encode_packet(const model::VesselState& state,
                                        const ViewSyncOptions& options, std::uint32_t counter);

}  // namespace nmeasim::core::viewsync
