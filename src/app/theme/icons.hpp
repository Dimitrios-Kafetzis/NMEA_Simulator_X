// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The toolbar and menu icons of the desktop application, painted in code.
///
/// `themed_icon` gives an icon in the colours of the current theme; `make_icon` paints one in
/// any colours. Painting in code keeps the symbols sharp at every size and lets them follow
/// the theme without image files. After `Theme::changed` the icons have to be requested
/// again, as `MainWindow` does.

#pragma once

#include <QColor>
#include <QIcon>

namespace nmeasim::app::theme {

/// The toolbar and menu symbols. They are painted in code, so they stay sharp at every size
/// and follow the colours of the current theme.
enum class Icon {
    NewProfile,   ///< A page with a plus sign: new profile.
    Open,         ///< A folder: open a profile.
    Save,         ///< A floppy disk: save the profile.
    Settings,     ///< A cog wheel: the settings dialog.
    Start,        ///< A filled triangle pointing right: start the simulation.
    Stop,         ///< A filled square: stop the simulation.
    Pause,        ///< Two filled bars: pause the simulation.
    Step,         ///< A triangle against a bar: pause and advance by one tick.
    Steering,     ///< A ship's wheel with six spokes: steering mode.
    Record,       ///< A filled circle, always in the danger colour: record a log.
    Track,        ///< A polyline through four points: open a track to follow.
    Log,          ///< Four bulleted lines: open a log file for replay.
    Follow,       ///< Cross hairs around a dot: keep the vessel centred on the map.
    Destination,  ///< A diamond around a dot: clear the destination.
};

/// Every `Icon` value, in declaration order, for tests and for regenerating icons after a
/// theme change.
inline constexpr Icon kAllIcons[]{Icon::NewProfile, Icon::Open,       Icon::Save,  Icon::Settings,
                                  Icon::Start,      Icon::Stop,       Icon::Pause, Icon::Step,
                                  Icon::Steering,   Icon::Record,     Icon::Track, Icon::Log,
                                  Icon::Follow,     Icon::Destination};

/// Paints an icon in the given colours.
///
/// The symbol is drawn with antialiasing on a transparent background, with round-capped
/// strokes 1.8 units wide in a 24 by 24 unit design grid. Pixmaps are rendered in advance at
/// 16, 20, 24 and 32 pixels, each at device pixel ratios 1 and 2 for HiDPI screens: 32
/// pixmaps in all, so the call is too costly for a paint event and its result is meant to be
/// kept, for example on a `QAction`.
///
/// @param icon Symbol to paint.
/// @param color Colour of the normal, unchecked state.
/// @param active Colour of the checked state of a checkable action (`QIcon::On`).
/// @param disabled Colour of the disabled state, checked or not.
/// @return The icon with every size, mode and state filled in.
[[nodiscard]] QIcon make_icon(Icon icon, const QColor& color, const QColor& active,
                              const QColor& disabled);

/// Paints an icon in the colours of the current theme.
///
/// Calls `make_icon` with `Colors::text` for the normal state, `Colors::accent` for the
/// checked state and `Colors::inactive` when disabled. `Icon::Record` is painted in
/// `Colors::danger`, checked or not, so that it always reads as a red record button.
///
/// @param icon Symbol to paint.
/// @return The icon; it keeps its colours when the theme changes later, so request it again
///   after `Theme::changed`.
[[nodiscard]] QIcon themed_icon(Icon icon);

}  // namespace nmeasim::app::theme
