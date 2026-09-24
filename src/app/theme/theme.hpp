// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The `Theme` singleton that applies the night-bridge or daylight look, and the colour
/// tables of both looks.
///
/// `Theme::apply` installs the Fusion style, a palette and a style sheet generated from a
/// `Colors` table; widgets that paint themselves read the same table through
/// `Theme::colors` and repaint on `Theme::changed`. The operator's choice is stored under
/// the setting key `appearance/theme` as the text of `to_string`.
///
/// @see docs/reference/desktop-app.md, section "Appearance".

#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>

/// The look of the desktop application: the `Theme` singleton with its night-bridge and
/// daylight looks and the option to follow the desktop, and the code-painted icons.
///
/// Each look is a `Colors` table of colours by role. `Theme::apply` turns the table into a
/// `QPalette` and, through `style_sheet`, into a Qt style sheet by replacing the `{token}`
/// placeholders of one template shared by both looks. Widgets that paint themselves (map,
/// dials, status lights, console highlighting) read the table directly and repaint when
/// `Theme::changed` is emitted. `make_icon` and `themed_icon` paint the toolbar and menu
/// symbols in the colours of the look, so no image files are needed. Part of the desktop
/// application, on Qt Widgets.
namespace nmeasim::app::theme {

/// What the operator chose under *View → Theme*.
///
/// Stored in the preferences under `appearance/theme` as the text given by `to_string`.
enum class Mode {
    /// Night or day, following the light or dark setting of the desktop; resolved by
    /// `Theme::resolve`. Stored as `system`.
    System,
    /// The dark *night bridge* look: charcoal panels, cyan accents, dimmed map tiles. The
    /// default. Stored as `night`.
    Night,
    /// The light *daylight* look: white panels, teal accents, map tiles as they are. Stored
    /// as `day`.
    Day,
};

/// Returns the name under which a mode is stored in the preferences.
///
/// @param mode Mode to name.
/// @return `system`, `night` or `day`; `night` for a value outside the enumeration.
[[nodiscard]] QString to_string(Mode mode);
/// Parses a mode name stored in the preferences.
///
/// The comparison is case-sensitive, as `to_string` writes lower case.
///
/// @param text Stored name: `system`, `night` or `day`.
/// @return The matching mode; `Mode::Night`, the default, for an empty or unknown name.
[[nodiscard]] Mode mode_from_string(const QString& text);

/// Every colour of one look, by role.
///
/// Widgets that paint themselves (map, dials, status lights, console highlighting) read them
/// from `Theme::colors`; everything else gets them through the palette and the style sheet
/// that `Theme::apply` installs. The two tables are fixed at compile time; see `colors_for`.
struct Colors {
    /// True for a dark look (night bridge), false for a light one (daylight). Chooses the
    /// opacity of the `{accent_soft}` style-sheet token and the map background.
    bool dark{true};
    /// Background of the main window, dialogs and the dashboard; also the colour of the
    /// overlay that dims map tiles.
    QColor window;
    /// Background of menus, tool bar, dock titles, status bar, instrument tiles, tab panes
    /// and table headers; the alternate row colour of tables.
    QColor panel;
    /// Background of raised elements: push buttons, tool tips, hovered menu-bar items and
    /// tool buttons, the dial faces.
    QColor panel_raised;
    /// Background of recessed areas: text views, tables, input fields (palette `Base`) and
    /// pressed buttons.
    QColor inset;
    /// Thin separators and frame outlines: tiles, group boxes, tables, splitters.
    QColor border;
    /// Stronger outlines: buttons, menus, tool tips, slider grooves and scroll-bar handles.
    QColor border_strong;
    /// Normal text.
    QColor text;
    /// Secondary text: captions, titles, headers, unlit status-light captions and
    /// placeholders.
    QColor text_dim;
    /// Emphasised values: instrument readouts and slider handles, brighter than `text`.
    QColor text_value;
    /// Accent: selection, checked and hovered controls, instrument units, links and the
    /// pointers of the dials.
    QColor accent;
    /// Text drawn on an `accent` background, such as selected menu items.
    QColor accent_text;
    /// Good state and starboard: a running simulation, open outputs, the starboard
    /// close-hauled sector of the wind dial.
    QColor ok;
    /// Caution: a paused simulation, an output that is opening, an overridden instrument.
    QColor warning;
    /// Fault, recording and port: a failed output, the recording light and icon, north on
    /// the compass rose, the port close-hauled sector of the wind dial.
    QColor danger;
    /// Unlit lights, closed outputs, disabled text and disabled icons.
    QColor inactive;
    /// Console: the start delimiter and talker identifier, for example `$GP`.
    QColor console_talker;
    /// Console: the sentence formatter, for example `RMC`, drawn in bold.
    QColor console_formatter;
    /// Console: the commas between fields.
    QColor console_separator;
    /// Console: the `*` and the two checksum digits.
    QColor console_checksum;
    /// Console: an IEC 61162-450 TAG block.
    QColor console_tag;
    /// Console: JSON messages such as Signal K deltas, coloured as a whole.
    QColor console_json;
    /// Map: the track sailed since the profile was applied.
    QColor map_track;
    /// Map: the loaded route or track being followed.
    QColor map_route;
    /// Map: the destination marker; also the bearing-to-destination marker on the compass
    /// rose.
    QColor map_destination;
    /// Map: fill of the vessel symbol.
    QColor map_vessel;
    /// Map: outline and halo of the vessel symbol; also the outline of markers and pointers
    /// on the map and the dials.
    QColor map_vessel_outline;
    /// Opacity of the `window`-coloured overlay painted over the map tiles, in [0, 1]:
    /// 0 keeps the tiles as they are. The night look uses 0.45 so that the chart does not
    /// dazzle next to the dark panels.
    double map_dimming{0.0};
};

/// Returns the colour table of a look.
///
/// @param mode `Mode::Night` or `Mode::Day`, or `Mode::System`, which is resolved from the
///   desktop's colour scheme with `Theme::resolve`.
/// @return The table of that look, valid for the lifetime of the program.
[[nodiscard]] const Colors& colors_for(Mode mode);

/// Generates the application style sheet of a look from its colours.
///
/// Every `{token}` of the template shared by both looks is replaced by a colour: the name of
/// a `Colors` member (`{window}`, `{panel}`, `{panel_raised}`, `{inset}`, `{border}`,
/// `{border_strong}`, `{text}`, `{text_dim}`, `{text_value}`, `{accent}`, `{accent_text}`,
/// `{warning}`, `{inactive}`) stands for that colour, `{accent_soft}` for `accent` at 22 %
/// opacity in a dark look and 15 % in a light one (checked tool buttons, table selection),
/// and `{panel_translucent}` for `panel` at 88 % opacity (buttons floating over the map).
/// Opaque colours are written as `#rrggbb`, translucent ones as `rgba(r, g, b, a)` with `a`
/// from 0 to 255.
///
/// Input fields (line edits, spin boxes, combo boxes) are deliberately left out and follow
/// the Fusion style and the palette: a style-sheet rule for them would also restyle the
/// editor inside every `QSpinBox` and drop its arrows.
///
/// @param colors Colours of the look.
/// @return The complete style sheet, with no `{token}` left.
[[nodiscard]] QString style_sheet(const Colors& colors);

/// Applies the looks to the application and tells custom-painted widgets when they change.
///
/// There is one instance, reached through `instance`. It starts in `Mode::Night` with the
/// night colours, but nothing is applied to the application until `apply` is called, which
/// `main` does at start-up with the mode read from `appearance/theme`. While the mode is
/// `Mode::System`, the instance re-applies the look whenever the desktop switches between
/// light and dark.
///
/// The instance lives in the GUI thread and is used only there.
class Theme : public QObject {
    Q_OBJECT

public:
    /// Returns the application-wide instance, creating it on the first call.
    ///
    /// The instance lives until the program exits. The constructor connects to the
    /// application's style hints to follow desktop colour-scheme changes, so the first call
    /// belongs after the `QApplication` is created, as in `main`.
    ///
    /// @return The single instance.
    [[nodiscard]] static Theme& instance();

    /// Applies a look to the running application: Fusion style, palette and style sheet.
    ///
    /// Resolves `Mode::System` with `resolve`. On the first call it installs the Fusion
    /// style. When the resolved look differs from the one in use, or on the first call, it
    /// sets the application palette from the colours of the resolved look
    /// (disabled text in `Colors::inactive`) and, when the application is a `QApplication`,
    /// its style sheet from `style_sheet`, and emits `changed`. Setting the application style
    /// sheet re-polishes every widget, which is why the look in use is not installed again.
    /// Does not store the choice in the preferences; the caller does.
    ///
    /// Emits `changed` synchronously before returning, and only when the look changed:
    /// choosing `Mode::System` while the desktop is dark and the night look is in use
    /// changes the mode but not the look.
    ///
    /// @param mode Mode to apply.
    /// @post `mode()` returns `mode` and `resolved()` the look now in use.
    void apply(Mode mode);
    /// Returns the mode last passed to `apply`, which may be `Mode::System`.
    ///
    /// @return The chosen mode; `Mode::Night` before the first `apply`.
    [[nodiscard]] Mode mode() const noexcept { return mode_; }
    /// Returns the look in use after resolving `Mode::System`.
    ///
    /// @return `Mode::Night` or `Mode::Day`; `Mode::Night` before the first `apply`.
    [[nodiscard]] Mode resolved() const noexcept { return resolved_; }
    /// Returns the colours of the look in use.
    ///
    /// @return The table of `resolved()`, valid for the lifetime of the program. It does not
    ///   follow later changes: read it again after `changed`.
    [[nodiscard]] const Colors& colors() const { return colors_for(resolved_); }

    /// Returns the font for large digital readouts: the bundled Share Tech Mono.
    ///
    /// The font is registered from the resource `:/fonts/ShareTechMono-Regular.ttf` on the
    /// first call; it is distributed under the SIL Open Font License 1.1. When it cannot be
    /// loaded, this and every later call fall back to the system's fixed-width font.
    ///
    /// @param point_size Size in points, greater than 0.
    /// @return A new font of that size.
    /// @pre A `QGuiApplication` exists.
    [[nodiscard]] static QFont readout_font(double point_size);
    /// Returns the fixed-width font for the console and other text columns.
    ///
    /// @return The system's fixed-width font at its default size, not Share Tech Mono.
    /// @pre A `QGuiApplication` exists.
    [[nodiscard]] static QFont mono_font();

    /// Resolves `Mode::System` from the desktop's colour scheme.
    ///
    /// @param mode Mode to resolve.
    /// @return `mode` unchanged when it is `Mode::Night` or `Mode::Day`. For `Mode::System`,
    ///   `Mode::Day` when the desktop reports a light scheme, otherwise `Mode::Night`,
    ///   including when the scheme is unknown or no style hints are available.
    [[nodiscard]] static Mode resolve(Mode mode);

signals:
    /// Emitted when the look in use changed: from `apply` when the operator chose a look that
    /// resolves to other colours, on the first `apply`, and when the desktop switched between
    /// light and dark while the mode is `Mode::System`. Not emitted when the look stays the
    /// same.
    ///
    /// Emitted synchronously from `apply`, after the palette and style sheet are installed,
    /// so receivers read the new `colors`. Connected widgets repaint or regenerate their
    /// icons.
    void changed();

private:
    /// Creates the instance and starts following desktop colour-scheme changes when the
    /// application provides style hints. Private: use `instance`.
    Theme();

    /// Mode last passed to `apply`; may be `Mode::System`.
    Mode mode_{Mode::Night};
    /// Look in use: `Mode::Night` or `Mode::Day`, never `Mode::System`.
    Mode resolved_{Mode::Night};
    /// Whether `apply` has installed a look; until then the application has neither the
    /// palette nor the style sheet of `resolved_`.
    bool applied_{false};
};

}  // namespace nmeasim::app::theme
