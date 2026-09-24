// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Derived quantities of the vessel state: magnetic and compass heading, magnetic course,
/// relative true wind.

#include <nmeasim/core/model/vessel_state.hpp>

namespace nmeasim::core::model {

double Navigation::heading_magnetic_deg() const noexcept {
    return geo::normalize_bearing(heading_true_deg - magnetic_variation_deg);
}

double Navigation::heading_compass_deg() const noexcept {
    return geo::normalize_bearing(heading_true_deg - magnetic_variation_deg -
                                  magnetic_deviation_deg);
}

double Navigation::course_over_ground_magnetic_deg() const noexcept {
    return geo::normalize_bearing(course_over_ground_deg - magnetic_variation_deg);
}

double Wind::true_angle_relative_deg(double heading_true_deg) const noexcept {
    return geo::normalize_bearing(true_direction_deg - heading_true_deg);
}

}  // namespace nmeasim::core::model
