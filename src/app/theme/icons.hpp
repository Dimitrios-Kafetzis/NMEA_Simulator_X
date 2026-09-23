#pragma once

#include <QColor>
#include <QIcon>

namespace nmeasim::app::theme {

/// The toolbar and menu symbols. They are painted in code, so they stay sharp at every size
/// and follow the colours of the current theme.
enum class Icon {
    NewProfile,
    Open,
    Save,
    Settings,
    Start,
    Stop,
    Pause,
    Step,
    Steering,
    Record,
    Track,
    Log,
    Follow,
    Destination,
};

/// Every icon kind, for tests and for regenerating icons after a theme change.
inline constexpr Icon kAllIcons[]{Icon::NewProfile, Icon::Open,       Icon::Save,  Icon::Settings,
                                  Icon::Start,      Icon::Stop,       Icon::Pause, Icon::Step,
                                  Icon::Steering,   Icon::Record,     Icon::Track, Icon::Log,
                                  Icon::Follow,     Icon::Destination};

/// Paints an icon in `color`, with a dimmed disabled state and `active` for the checked
/// state of checkable actions, at 16, 20, 24 and 32 pixels and twice those for HiDPI screens.
[[nodiscard]] QIcon make_icon(Icon icon, const QColor& color, const QColor& active,
                              const QColor& disabled);

/// `make_icon` with the colours of the current theme; the record symbol is always red.
[[nodiscard]] QIcon themed_icon(Icon icon);

}  // namespace nmeasim::app::theme
