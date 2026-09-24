// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The standard sentence catalogue and the encoding within the NMEA 0183 length limit.
///
/// Implements the functions declared in `registry.hpp`. The table in
/// `SentenceRegistry::standard` holds the ids, default talkers, periods and descriptions
/// documented on the sentence reference page; changing an entry changes the output.

#include <nmeasim/core/nmea0183/registry.hpp>
#include <nmeasim/core/nmea0183/sentence_builder.hpp>

#include <algorithm>

namespace nmeasim::core::nmea0183 {

using namespace std::chrono_literals;

std::string_view to_string(SentenceGroup group) noexcept {
    switch (group) {
        case SentenceGroup::Gnss:
            return "GNSS";
        case SentenceGroup::Time:
            return "Time";
        case SentenceGroup::Heading:
            return "Heading";
        case SentenceGroup::Speed:
            return "Speed";
        case SentenceGroup::Depth:
            return "Depth";
        case SentenceGroup::Wind:
            return "Wind";
        case SentenceGroup::Steering:
            return "Steering";
        case SentenceGroup::Autopilot:
            return "Autopilot";
        case SentenceGroup::Propulsion:
            return "Propulsion";
        case SentenceGroup::Ais:
            return "AIS";
    }
    return "Unknown";
}

const SentenceRegistry& SentenceRegistry::standard() {
    // The VDM entries repeat the VDO reports, framed as received, for consumers that ignore
    // VDO; they are therefore off by default (see ADR 0013 and the AIS reference page).
    static const SentenceRegistry registry{{
        {"RMC", "RMC", "GP", SentenceGroup::Gnss, 1000ms, true,
         "Recommended minimum navigation data: time, position, speed, course, date, variation",
         &encode_rmc},
        {"GGA", "GGA", "GP", SentenceGroup::Gnss, 1000ms, true,
         "GNSS fix data: time, position, fix quality, satellites, HDOP, altitude", &encode_gga},
        {"GLL", "GLL", "GP", SentenceGroup::Gnss, 1000ms, true,
         "Geographic position: latitude, longitude, time, status", &encode_gll},
        {"GSA", "GSA", "GP", SentenceGroup::Gnss, 1000ms, true,
         "Active satellites and dilution of precision", &encode_gsa},
        {"GSV", "GSV", "GP", SentenceGroup::Gnss, 1000ms, true, "Satellites in view", &encode_gsv},
        {"VTG", "VTG", "GP", SentenceGroup::Gnss, 1000ms, true,
         "Course over ground and ground speed", &encode_vtg},
        {"ZDA", "ZDA", "GP", SentenceGroup::Time, 1000ms, true, "UTC time and date", &encode_zda},
        {"HDG", "HDG", "HC", SentenceGroup::Heading, 1000ms, true,
         "Magnetic heading with deviation and variation", &encode_hdg},
        {"HDM", "HDM", "HC", SentenceGroup::Heading, 1000ms, true, "Magnetic heading", &encode_hdm},
        {"HDT", "HDT", "HE", SentenceGroup::Heading, 1000ms, true, "True heading", &encode_hdt},
        {"ROT", "ROT", "TI", SentenceGroup::Heading, 1000ms, true, "Rate of turn", &encode_rot},
        {"VHW", "VHW", "VW", SentenceGroup::Speed, 1000ms, true, "Water speed and heading",
         &encode_vhw},
        {"VBW", "VBW", "VW", SentenceGroup::Speed, 1000ms, true, "Dual ground and water speed",
         &encode_vbw},
        {"DPT", "DPT", "SD", SentenceGroup::Depth, 1000ms, true,
         "Depth below transducer with offset", &encode_dpt},
        {"DBT", "DBT", "SD", SentenceGroup::Depth, 1000ms, true,
         "Depth below transducer in feet, metres and fathoms", &encode_dbt},
        {"MTW", "MTW", "YC", SentenceGroup::Depth, 1000ms, true, "Water temperature", &encode_mtw},
        {"MWV-R", "MWV", "WI", SentenceGroup::Wind, 1000ms, true, "Apparent wind angle and speed",
         &encode_mwv_apparent},
        {"MWV-T", "MWV", "WI", SentenceGroup::Wind, 1000ms, false,
         "True wind angle relative to the bow and true wind speed", &encode_mwv_true},
        {"MWD", "MWD", "WI", SentenceGroup::Wind, 1000ms, true, "True wind direction and speed",
         &encode_mwd},
        {"RSA", "RSA", "II", SentenceGroup::Steering, 1000ms, true, "Rudder angle", &encode_rsa},
        {"APB", "APB", "GP", SentenceGroup::Autopilot, 1000ms, true,
         "Autopilot sentence B: cross-track error, bearings and arrival for the destination",
         &encode_apb},
        {"RMB", "RMB", "GP", SentenceGroup::Autopilot, 1000ms, true,
         "Recommended minimum navigation to the destination: cross-track error, range, bearing",
         &encode_rmb},
        {"XTE", "XTE", "GP", SentenceGroup::Autopilot, 1000ms, true,
         "Cross-track error from the leg to the destination", &encode_xte},
        {"RPM", "RPM", "ER", SentenceGroup::Propulsion, 1000ms, true,
         "Engine revolutions, one sentence per engine", &encode_rpm},
        {"XDR", "XDR", "ER", SentenceGroup::Propulsion, 1000ms, true,
         "Transducer measurements: engine coolant temperature and tachometer per engine",
         &encode_xdr},
        {"VDO-POS", "VDO", "AI", SentenceGroup::Ais, 2000ms, true,
         "AIS own-vessel position report (message type 1, 2 or 3)", &encode_vdo_position},
        {"VDO-STATIC", "VDO", "AI", SentenceGroup::Ais, 30000ms, true,
         "AIS own-vessel static and voyage data (message type 5)", &encode_vdo_static},
        {"VDM-POS", "VDM", "AI", SentenceGroup::Ais, 2000ms, false,
         "The own-vessel position report framed as a received message", &encode_vdm_position},
        {"VDM-STATIC", "VDM", "AI", SentenceGroup::Ais, 30000ms, false,
         "The own-vessel static data framed as a received message", &encode_vdm_static},
    }};
    return registry;
}

SentenceRegistry::SentenceRegistry(std::vector<SentenceDescriptor> descriptors)
    : descriptors_(std::move(descriptors)) {}

std::span<const SentenceDescriptor> SentenceRegistry::descriptors() const noexcept {
    return descriptors_;
}

const SentenceDescriptor* SentenceRegistry::find(std::string_view id) const noexcept {
    const auto it = std::ranges::find(descriptors_, id, &SentenceDescriptor::id);
    return it == descriptors_.end() ? nullptr : &*it;
}

std::vector<std::string> encode_within_limit(const SentenceDescriptor& descriptor,
                                             const model::VesselState& state,
                                             std::string_view talker, EncoderOptions options) {
    // Two decimals of a minute still resolve about 18 m; the profile never goes below this.
    constexpr int kMinimumPositionDecimals = 2;
    std::vector<std::string> sentences;
    for (int decimals = options.position_decimals; decimals >= kMinimumPositionDecimals;
         --decimals) {
        options.position_decimals = decimals;
        // An empty result (nothing to report) also satisfies all_of and ends the loop.
        sentences = descriptor.encoder(EncoderContext{state, talker, options});
        if (std::ranges::all_of(sentences, [](const std::string& s) { return fits_limit(s); })) {
            break;
        }
    }
    return sentences;
}

}  // namespace nmeasim::core::nmea0183
