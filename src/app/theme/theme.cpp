#include "theme.hpp"

#include <QApplication>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

#include <utility>
#include <vector>

namespace nmeasim::app::theme {

namespace {

Colors night_colors() {
    Colors c;
    c.dark = true;
    c.window = QColor(0x0f, 0x17, 0x20);
    c.panel = QColor(0x16, 0x21, 0x2c);
    c.panel_raised = QColor(0x1d, 0x2a, 0x37);
    c.inset = QColor(0x0a, 0x11, 0x18);
    c.border = QColor(0x22, 0x31, 0x3f);
    c.border_strong = QColor(0x31, 0x46, 0x5a);
    c.text = QColor(0xd8, 0xe3, 0xec);
    c.text_dim = QColor(0x7f, 0x93, 0xa6);
    c.text_value = QColor(0xf2, 0xfb, 0xff);
    c.accent = QColor(0x22, 0xc3, 0xe6);
    c.accent_text = QColor(0x04, 0x14, 0x1b);
    c.ok = QColor(0x3d, 0xdc, 0x84);
    c.warning = QColor(0xf5, 0xb8, 0x3d);
    c.danger = QColor(0xff, 0x5c, 0x5c);
    c.inactive = QColor(0x4a, 0x5b, 0x6b);
    c.console_talker = QColor(0x7f, 0x93, 0xa6);
    c.console_formatter = QColor(0x22, 0xc3, 0xe6);
    c.console_separator = QColor(0x4a, 0x5b, 0x6b);
    c.console_checksum = QColor(0xf5, 0xb8, 0x3d);
    c.console_tag = QColor(0xb1, 0x8c, 0xff);
    c.console_json = QColor(0x8f, 0xd6, 0x94);
    c.map_track = QColor(0xff, 0x6b, 0x6b);
    c.map_route = QColor(0x3d, 0xdc, 0x84);
    c.map_destination = QColor(0xe0, 0x5a, 0xd6);
    c.map_vessel = QColor(0xf5, 0xd5, 0x47);
    c.map_vessel_outline = QColor(0x0a, 0x11, 0x18);
    c.map_dimming = 0.45;
    return c;
}

Colors day_colors() {
    Colors c;
    c.dark = false;
    c.window = QColor(0xee, 0xf2, 0xf5);
    c.panel = QColor(0xff, 0xff, 0xff);
    c.panel_raised = QColor(0xf7, 0xf9, 0xfb);
    c.inset = QColor(0xf4, 0xf7, 0xf9);
    c.border = QColor(0xd5, 0xdd, 0xe5);
    c.border_strong = QColor(0xb9, 0xc6, 0xd2);
    c.text = QColor(0x1b, 0x27, 0x33);
    c.text_dim = QColor(0x5d, 0x6f, 0x80);
    c.text_value = QColor(0x0b, 0x1a, 0x26);
    c.accent = QColor(0x0b, 0x7f, 0xa6);
    c.accent_text = QColor(0xff, 0xff, 0xff);
    c.ok = QColor(0x1e, 0x9e, 0x5a);
    c.warning = QColor(0xc9, 0x8a, 0x00);
    c.danger = QColor(0xd6, 0x45, 0x45);
    c.inactive = QColor(0xa3, 0xb1, 0xbd);
    c.console_talker = QColor(0x5d, 0x6f, 0x80);
    c.console_formatter = QColor(0x0b, 0x7f, 0xa6);
    c.console_separator = QColor(0xa3, 0xb1, 0xbd);
    c.console_checksum = QColor(0xb3, 0x6b, 0x00);
    c.console_tag = QColor(0x7a, 0x4f, 0xd1);
    c.console_json = QColor(0x2f, 0x8a, 0x4a);
    c.map_track = QColor(0xd6, 0x3a, 0x3a);
    c.map_route = QColor(0x1e, 0x9e, 0x5a);
    c.map_destination = QColor(0xb0, 0x20, 0x9a);
    c.map_vessel = QColor(0xf0, 0xc0, 0x20);
    c.map_vessel_outline = QColor(0x1b, 0x27, 0x33);
    c.map_dimming = 0.0;
    return c;
}

QString css(const QColor& color) {
    return color.alpha() == 255 ? color.name(QColor::HexRgb)
                                : QStringLiteral("rgba(%1, %2, %3, %4)")
                                      .arg(color.red())
                                      .arg(color.green())
                                      .arg(color.blue())
                                      .arg(color.alpha());
}

QColor with_alpha(QColor color, int alpha) {
    color.setAlpha(alpha);
    return color;
}

// Style sheet shared by both looks; {tokens} are replaced by the colours of the look. Input
// fields (line edits, spin boxes, combo boxes) are left to the Fusion style and the palette:
// styling them here would also restyle the editor inside every spin box and drop its arrows.
constexpr const char* kStyleTemplate = R"css(
QMainWindow, QDialog { background: {window}; }
QToolTip { background: {panel_raised}; color: {text}; border: 1px solid {border_strong}; padding: 4px; }
QMenuBar { background: {panel}; border-bottom: 1px solid {border}; padding: 2px; }
QMenuBar::item { background: transparent; padding: 4px 10px; border-radius: 4px; }
QMenuBar::item:selected { background: {panel_raised}; }
QMenu { background: {panel}; border: 1px solid {border_strong}; padding: 4px; }
QMenu::item { padding: 5px 28px 5px 24px; border-radius: 4px; }
QMenu::item:selected { background: {accent}; color: {accent_text}; }
QMenu::item:disabled { color: {inactive}; }
QMenu::separator { height: 1px; background: {border}; margin: 4px 8px; }
QToolBar { background: {panel}; border: none; border-bottom: 1px solid {border}; padding: 4px 6px; spacing: 2px; }
QToolBar::separator { background: {border}; width: 1px; margin: 5px 6px; }
QToolButton { color: {text}; border: 1px solid transparent; border-radius: 6px; padding: 4px 8px; }
QToolButton:hover { background: {panel_raised}; border-color: {border_strong}; }
QToolButton:pressed { background: {inset}; }
QToolButton:checked { background: {accent_soft}; border-color: {accent}; }
QToolButton:disabled { color: {inactive}; }
QDockWidget { color: {text_dim}; }
QDockWidget::title { background: {panel}; padding: 6px 10px; border-bottom: 1px solid {border}; text-align: left; }
QStatusBar { background: {panel}; border-top: 1px solid {border}; color: {text_dim}; }
QStatusBar::item { border: none; }
QFrame#instrument_tile, QFrame#engine_tile, QFrame#dial_panel { background: {panel}; border: 1px solid {border}; border-radius: 8px; }
QFrame#instrument_tile[overridden="true"] { border: 1px solid {warning}; }
QLabel#tile_title { color: {text_dim}; font-weight: 600; }
QLabel#tile_value { color: {text_value}; }
QLabel#tile_unit { color: {accent}; font-weight: 600; }
QLabel#section_title { color: {text_dim}; font-weight: 600; }
QPlainTextEdit, QTextEdit { background: {inset}; color: {text}; border: 1px solid {border}; border-radius: 4px; selection-background-color: {accent}; selection-color: {accent_text}; }
QTableView, QTableWidget, QListView, QTreeView { background: {inset}; alternate-background-color: {panel}; color: {text}; border: 1px solid {border}; border-radius: 4px; gridline-color: {border}; selection-background-color: {accent_soft}; selection-color: {text}; }
QHeaderView::section { background: {panel}; color: {text_dim}; border: none; border-bottom: 1px solid {border}; border-right: 1px solid {border}; padding: 4px 6px; font-weight: 600; }
QTableCornerButton::section { background: {panel}; border: none; }
QTabWidget::pane { border: 1px solid {border}; border-radius: 6px; top: -1px; background: {panel}; }
QTabBar::tab { background: transparent; color: {text_dim}; padding: 6px 14px; border: none; border-bottom: 2px solid transparent; }
QTabBar::tab:selected { color: {text}; border-bottom: 2px solid {accent}; }
QTabBar::tab:hover { color: {text}; }
QPushButton { background: {panel_raised}; color: {text}; border: 1px solid {border_strong}; border-radius: 5px; padding: 5px 14px; }
QPushButton:hover { border-color: {accent}; }
QPushButton:pressed { background: {inset}; }
QPushButton:default { border-color: {accent}; }
QPushButton:disabled { color: {inactive}; border-color: {border}; }
QGroupBox { border: 1px solid {border}; border-radius: 6px; margin-top: 14px; padding-top: 8px; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 4px; color: {text_dim}; font-weight: 600; }
QSlider::groove:horizontal { height: 4px; background: {border_strong}; border-radius: 2px; }
QSlider::sub-page:horizontal { background: {accent}; border-radius: 2px; }
QSlider::sub-page:horizontal:disabled { background: {border_strong}; }
QSlider::handle:horizontal { background: {text_value}; border: 2px solid {accent}; width: 10px; height: 10px; margin: -5px 0; border-radius: 7px; }
QSlider::handle:horizontal:disabled { background: {inactive}; border-color: {inactive}; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:vertical { background: {border_strong}; border-radius: 3px; min-height: 24px; margin: 2px; }
QScrollBar::handle:horizontal { background: {border_strong}; border-radius: 3px; min-width: 24px; margin: 2px; }
QScrollBar::handle:hover { background: {text_dim}; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
QSplitter::handle { background: {border}; }
QToolButton#map_button { background: {panel_translucent}; color: {text}; border: 1px solid {border_strong}; border-radius: 6px; font-size: 13pt; font-weight: 700; padding: 0; }
QToolButton#map_button:hover { border-color: {accent}; }
QToolButton#map_button:checked { background: {accent_soft}; border-color: {accent}; }
QScrollArea#dashboard_scroll { background: transparent; border: none; }
QWidget#dashboard_content { background: {window}; }
QMainWindow::separator { background: {border}; width: 1px; height: 1px; }
)css";

}  // namespace

QString to_string(Mode mode) {
    switch (mode) {
        case Mode::System:
            return QStringLiteral("system");
        case Mode::Night:
            return QStringLiteral("night");
        case Mode::Day:
            return QStringLiteral("day");
    }
    return QStringLiteral("night");
}

Mode mode_from_string(const QString& text) {
    if (text == QLatin1String("system")) {
        return Mode::System;
    }
    if (text == QLatin1String("day")) {
        return Mode::Day;
    }
    return Mode::Night;
}

const Colors& colors_for(Mode resolved) {
    static const Colors night = night_colors();
    static const Colors day = day_colors();
    return resolved == Mode::Day ? day : night;
}

QString style_sheet(const Colors& colors) {
    const std::vector<std::pair<const char*, QColor>> tokens{
        {"{window}", colors.window},
        {"{panel_raised}", colors.panel_raised},
        {"{panel}", colors.panel},
        {"{inset}", colors.inset},
        {"{border_strong}", colors.border_strong},
        {"{border}", colors.border},
        {"{text_value}", colors.text_value},
        {"{text_dim}", colors.text_dim},
        {"{text}", colors.text},
        {"{accent_soft}", with_alpha(colors.accent, colors.dark ? 0x38 : 0x26)},
        {"{panel_translucent}", with_alpha(colors.panel, 0xe0)},
        {"{accent_text}", colors.accent_text},
        {"{accent}", colors.accent},
        {"{warning}", colors.warning},
        {"{inactive}", colors.inactive},
    };
    QString sheet = QString::fromLatin1(kStyleTemplate);
    for (const auto& [token, color] : tokens) {
        sheet.replace(QLatin1String(token), css(color));
    }
    return sheet;
}

Theme& Theme::instance() {
    static Theme theme;
    return theme;
}

Theme::Theme() {
    if (auto* hints = QGuiApplication::styleHints()) {
        connect(hints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            if (mode_ == Mode::System) {
                apply(Mode::System);
            }
        });
    }
}

Mode Theme::resolve(Mode mode) {
    if (mode != Mode::System) {
        return mode;
    }
    const auto* hints = QGuiApplication::styleHints();
    if (hints != nullptr && hints->colorScheme() == Qt::ColorScheme::Light) {
        return Mode::Day;
    }
    return Mode::Night;
}

void Theme::apply(Mode mode) {
    mode_ = mode;
    resolved_ = resolve(mode);
    const Colors& c = colors_for(resolved_);

    if (auto* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        QApplication::setStyle(fusion);
    }
    QPalette palette;
    palette.setColor(QPalette::Window, c.window);
    palette.setColor(QPalette::WindowText, c.text);
    palette.setColor(QPalette::Base, c.inset);
    palette.setColor(QPalette::AlternateBase, c.panel);
    palette.setColor(QPalette::Text, c.text);
    palette.setColor(QPalette::BrightText, c.text_value);
    palette.setColor(QPalette::Button, c.panel_raised);
    palette.setColor(QPalette::ButtonText, c.text);
    palette.setColor(QPalette::Highlight, c.accent);
    palette.setColor(QPalette::HighlightedText, c.accent_text);
    palette.setColor(QPalette::ToolTipBase, c.panel_raised);
    palette.setColor(QPalette::ToolTipText, c.text);
    palette.setColor(QPalette::PlaceholderText, c.text_dim);
    palette.setColor(QPalette::Link, c.accent);
    palette.setColor(QPalette::Light, c.border_strong);
    palette.setColor(QPalette::Midlight, c.border);
    palette.setColor(QPalette::Mid, c.border);
    palette.setColor(QPalette::Dark, c.inset);
    palette.setColor(QPalette::Shadow, c.inset);
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        palette.setColor(QPalette::Disabled, role, c.inactive);
    }
    QApplication::setPalette(palette);
    if (auto* application = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        application->setStyleSheet(style_sheet(c));
    }
    emit changed();
}

QFont Theme::readout_font(double point_size) {
    static const QString family = [] {
        const int id =
            QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/ShareTechMono-Regular.ttf"));
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        return families.isEmpty() ? QString{} : families.first();
    }();
    QFont font =
        family.isEmpty() ? QFontDatabase::systemFont(QFontDatabase::FixedFont) : QFont(family);
    font.setPointSizeF(point_size);
    return font;
}

QFont Theme::mono_font() {
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

}  // namespace nmeasim::app::theme
