#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>

namespace nmeasim::app::theme {

/// What the operator chose under *View → Theme*.
enum class Mode {
    /// Night or day, following the light or dark setting of the desktop.
    System,
    /// The dark *night bridge* look: charcoal panels, cyan accents.
    Night,
    /// The light *daylight* look: white panels, teal accents.
    Day,
};

/// Name stored in the preferences: `system`, `night` or `day`.
[[nodiscard]] QString to_string(Mode mode);
/// Parses a stored name; anything unknown gives `Mode::Night`, the default.
[[nodiscard]] Mode mode_from_string(const QString& text);

/// Every colour the interface uses. Widgets that paint themselves (map, dials, status lights,
/// console highlighting) read them from `Theme::colors()`; everything else gets them through
/// the palette and the style sheet.
struct Colors {
    bool dark{true};
    QColor window;
    QColor panel;
    QColor panel_raised;
    QColor inset;
    QColor border;
    QColor border_strong;
    QColor text;
    QColor text_dim;
    QColor text_value;
    QColor accent;
    QColor accent_text;
    QColor ok;
    QColor warning;
    QColor danger;
    QColor inactive;
    /// Console highlighting: talker, sentence formatter, field separators, checksum, TAG block
    /// and JSON messages (Signal K, ViewSync).
    QColor console_talker;
    QColor console_formatter;
    QColor console_separator;
    QColor console_checksum;
    QColor console_tag;
    QColor console_json;
    /// Map: sailed track, loaded route, destination, vessel fill and outline, and how strongly
    /// tiles are darkened (0 keeps them as they are).
    QColor map_track;
    QColor map_route;
    QColor map_destination;
    QColor map_vessel;
    QColor map_vessel_outline;
    double map_dimming{0.0};
};

/// The colours of a concrete look; `Mode::System` is resolved first.
[[nodiscard]] const Colors& colors_for(Mode resolved);

/// The style sheet of a look, generated from its colours.
[[nodiscard]] QString style_sheet(const Colors& colors);

/// Applies the looks to the application and tells custom-painted widgets when they change.
class Theme : public QObject {
    Q_OBJECT

public:
    /// The application-wide instance; created on first use.
    [[nodiscard]] static Theme& instance();

    /// Applies a look to the running application: Fusion style, palette and style sheet.
    void apply(Mode mode);
    [[nodiscard]] Mode mode() const noexcept { return mode_; }
    /// `Night` or `Day`: the look in use after resolving `System`.
    [[nodiscard]] Mode resolved() const noexcept { return resolved_; }
    [[nodiscard]] const Colors& colors() const { return colors_for(resolved_); }

    /// Font for large digital readouts: the bundled Share Tech Mono, or the system's fixed
    /// font when it cannot be loaded.
    [[nodiscard]] static QFont readout_font(double point_size);
    /// Fixed-width font for the console and other text columns.
    [[nodiscard]] static QFont mono_font();

    /// Resolves `System` from the desktop's colour scheme.
    [[nodiscard]] static Mode resolve(Mode mode);

signals:
    /// The colours changed, because the operator chose another look or the desktop switched
    /// between light and dark while following the system.
    void changed();

private:
    Theme();

    Mode mode_{Mode::Night};
    Mode resolved_{Mode::Night};
};

}  // namespace nmeasim::app::theme
