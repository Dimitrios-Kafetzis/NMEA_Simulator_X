// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of `nmeasim::app::theme::Theme` and the themed parts of the desktop application.
///
/// Covers the console colouring by `nmeasim::app::sentence_spans`, theme names, applying a
/// theme to the application palette and style sheet only when the look changes, the system
/// look following the desktop colour scheme where the platform lets the test choose it, the bundled
/// readout font, the toolbar icons, `nmeasim::app::StatusLed`, the override mark of
/// `nmeasim::app::InstrumentTile`, the colours of output states, and the status lights and theme
/// menu of `nmeasim::app::MainWindow`. The theme is a process-wide singleton, so a test that
/// depends on one theme applies it first. The last test stores the theme choice through
/// `QSettings`, which `main.cpp` redirects to a temporary directory. The file reads no fixtures.

#include "theme/theme.hpp"

#include "io/event_loop.hpp"
#include "main_window.hpp"
#include "theme/icons.hpp"
#include "widgets/instrument_tile.hpp"
#include "widgets/outputs_widget.hpp"
#include "widgets/sentence_highlighter.hpp"
#include "widgets/status_led.hpp"

#include <QAction>
#include <QApplication>
#include <QFontInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPalette>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QStyle>
#include <QStyleHints>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace theme = nmeasim::app::theme;
using nmeasim::app::SentenceRole;
using nmeasim::app::SentenceSpan;

namespace {

/// Splits a console line into coloured runs.
///
/// @param line The line as Latin-1 text, without a line terminator.
/// @return The runs `nmeasim::app::sentence_spans` finds, in order of their start.
std::vector<SentenceSpan> spans(const char* line) {
    return nmeasim::app::sentence_spans(QString::fromLatin1(line));
}

}  // namespace

TEST_CASE("console lines are split into coloured runs", "[app][theme]") {
    // Starts and lengths are character indices into the literal: the talker includes the `$`
    // or `!`, and the checksum includes the `*`.
    CHECK(spans("$GPRMC,100000.10,A,3759.0281,N*0D") ==
          std::vector<SentenceSpan>{{0, 3, SentenceRole::Talker},
                                    {3, 3, SentenceRole::Formatter},
                                    {6, 1, SentenceRole::Separator},
                                    {16, 1, SentenceRole::Separator},
                                    {18, 1, SentenceRole::Separator},
                                    {28, 1, SentenceRole::Separator},
                                    {30, 3, SentenceRole::Checksum}});
    CHECK(spans("!AIVDO,1,1,,A,13SsIh@4i11dWJdEg0RAhQJ00000,0*65") ==
          std::vector<SentenceSpan>{{0, 3, SentenceRole::Talker},
                                    {3, 3, SentenceRole::Formatter},
                                    {6, 1, SentenceRole::Separator},
                                    {8, 1, SentenceRole::Separator},
                                    {10, 1, SentenceRole::Separator},
                                    {11, 1, SentenceRole::Separator},
                                    {13, 1, SentenceRole::Separator},
                                    {42, 1, SentenceRole::Separator},
                                    {44, 3, SentenceRole::Checksum}});
    // The whole TAG block, both backslashes included, is one run of 27 characters; the commas
    // inside it are not separators.
    CHECK(spans("\\s:SIM0001,c:1790000000*5B\\$GPGGA,1,2*59") ==
          std::vector<SentenceSpan>{{0, 27, SentenceRole::Tag},
                                    {27, 3, SentenceRole::Talker},
                                    {30, 3, SentenceRole::Formatter},
                                    {33, 1, SentenceRole::Separator},
                                    {35, 1, SentenceRole::Separator},
                                    {37, 3, SentenceRole::Checksum}});
    // Proprietary sentences: $P is the talker, the rest of the address the formatter.
    CHECK(spans("$PGRMZ,93,f,3*21") == std::vector<SentenceSpan>{{0, 2, SentenceRole::Talker},
                                                                 {2, 4, SentenceRole::Formatter},
                                                                 {6, 1, SentenceRole::Separator},
                                                                 {9, 1, SentenceRole::Separator},
                                                                 {11, 1, SentenceRole::Separator},
                                                                 {13, 3, SentenceRole::Checksum}});
    CHECK(spans("{\"context\":\"vessels.self\"}") ==
          std::vector<SentenceSpan>{{0, 26, SentenceRole::Json}});
    CHECK(spans("").empty());
    CHECK(spans("hello, world").empty());
    CHECK(spans("\\unterminated tag").empty());
    // A missing or malformed checksum is simply not coloured.
    CHECK(spans("$GPGLL,1*Z").back().role == SentenceRole::Separator);
    // A checksum is two ASCII hexadecimal digits: ARABIC-INDIC DIGIT THREE (U+0663) is a
    // decimal digit, but not one of an NMEA 0183 checksum.
    CHECK(nmeasim::app::sentence_spans(QStringLiteral("$GPGLL,1*\u06634")).back().role ==
          SentenceRole::Separator);
}

TEST_CASE("theme names round-trip and unknown names give the night theme", "[app][theme]") {
    for (const auto mode : {theme::Mode::System, theme::Mode::Night, theme::Mode::Day}) {
        CHECK(theme::mode_from_string(theme::to_string(mode)) == mode);
    }
    CHECK(theme::mode_from_string(QStringLiteral("sepia")) == theme::Mode::Night);
    CHECK(theme::Theme::resolve(theme::Mode::Day) == theme::Mode::Day);
    const auto system = theme::Theme::resolve(theme::Mode::System);
    CHECK((system == theme::Mode::Night || system == theme::Mode::Day));
}

TEST_CASE("applying a theme sets the palette and the style sheet", "[app][theme]") {
    auto& instance = theme::Theme::instance();
    // Starts from the night look, which an earlier test may have left, so that Day changes it.
    instance.apply(theme::Mode::Night);
    QSignalSpy changed(&instance, &theme::Theme::changed);

    instance.apply(theme::Mode::Day);
    CHECK(changed.count() == 1);
    CHECK(instance.resolved() == theme::Mode::Day);
    CHECK_FALSE(instance.colors().dark);
    CHECK(QApplication::palette().color(QPalette::Window) ==
          theme::colors_for(theme::Mode::Day).window);
    CHECK(qApp->styleSheet().contains(theme::colors_for(theme::Mode::Day).accent.name()));

    instance.apply(theme::Mode::Night);
    CHECK(changed.count() == 2);
    CHECK(instance.colors().dark);
    CHECK(QApplication::palette().color(QPalette::Highlight) ==
          theme::colors_for(theme::Mode::Night).accent);

    // Every colour token of the template is replaced.
    for (const auto mode : {theme::Mode::Night, theme::Mode::Day}) {
        const QString sheet = theme::style_sheet(theme::colors_for(mode));
        CHECK_FALSE(sheet.contains(QStringLiteral("{window}")));
        CHECK_FALSE(sheet.contains(QRegularExpression(QStringLiteral("\\{[a-z_]+\\}"))));
    }
}

TEST_CASE("applying the look in use again changes nothing", "[app][theme]") {
    auto& instance = theme::Theme::instance();
    instance.apply(theme::Mode::Night);
    // The application owns its style; with a style sheet set, QApplication::style returns a
    // style-sheet proxy, recreated by every style sheet, that owns the Fusion style in turn.
    const auto fusion = [] {
        for (const auto* style : qApp->findChildren<QStyle*>()) {
            if (style->name().compare(QLatin1String("fusion"), Qt::CaseInsensitive) == 0) {
                return style;
            }
        }
        return static_cast<const QStyle*>(nullptr);
    };
    const QStyle* style = fusion();
    REQUIRE(style != nullptr);
    QSignalSpy changed(&instance, &theme::Theme::changed);

    instance.apply(theme::Mode::Night);
    CHECK(changed.count() == 0);
    CHECK(fusion() == style);

    // Another look is applied and announced, on the same Fusion style.
    instance.apply(theme::Mode::Day);
    CHECK(changed.count() == 1);
    CHECK(fusion() == style);
    CHECK(QApplication::palette().color(QPalette::Window) ==
          theme::colors_for(theme::Mode::Day).window);
    instance.apply(theme::Mode::Night);
    CHECK(changed.count() == 2);
}

TEST_CASE("the system look follows the desktop colour scheme", "[app][theme]") {
    auto* hints = QGuiApplication::styleHints();
    REQUIRE(hints != nullptr);
    const Qt::ColorScheme before = hints->colorScheme();
    for (const auto scheme : {Qt::ColorScheme::Light, Qt::ColorScheme::Dark}) {
        hints->setColorScheme(scheme);
        if (hints->colorScheme() != scheme) {
            // The platform does not let the application choose its scheme.
            continue;
        }
        const bool light = scheme == Qt::ColorScheme::Light;
        CHECK(theme::Theme::resolve(theme::Mode::System) ==
              (light ? theme::Mode::Day : theme::Mode::Night));
        CHECK(theme::colors_for(theme::Mode::System).dark == !light);
    }
    hints->setColorScheme(before);
    // Whatever the platform, the system colours are those of the resolved look.
    CHECK(&theme::colors_for(theme::Mode::System) ==
          &theme::colors_for(theme::Theme::resolve(theme::Mode::System)));
}

TEST_CASE("the readout font is the bundled Share Tech Mono", "[app][theme]") {
    const QFont font = theme::Theme::readout_font(20.0);
    CHECK(font.family() == QStringLiteral("Share Tech Mono"));
    CHECK(font.pointSizeF() == 20.0);
}

TEST_CASE("every icon is painted in both states", "[app][theme]") {
    for (const auto kind : theme::kAllIcons) {
        const QIcon icon = theme::make_icon(kind, Qt::white, Qt::cyan, Qt::gray);
        REQUIRE_FALSE(icon.isNull());
        for (const auto state : {QIcon::Off, QIcon::On}) {
            const QImage image = icon.pixmap(QSize(24, 24), 1.0, QIcon::Normal, state).toImage();
            int painted = 0;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    painted += qAlpha(image.pixel(x, y)) > 0 ? 1 : 0;
                }
            }
            CHECK(painted > 20);
        }
    }
}

TEST_CASE("status lights show a colour, a caption and blink", "[app][theme]") {
    nmeasim::app::StatusLed led;
    led.set_state(Qt::green, QStringLiteral("RUNNING"));
    CHECK(led.color() == QColor(Qt::green));
    CHECK(led.text() == QStringLiteral("RUNNING"));
    CHECK(led.sizeHint().width() > 40);
    CHECK_FALSE(led.blinking());
    led.set_blinking(true);
    CHECK(led.blinking());
    led.set_blinking(false);
    CHECK_FALSE(led.blinking());
    led.resize(led.sizeHint());
    CHECK_FALSE(led.grab().isNull());
}

TEST_CASE("an active override marks the instrument tile", "[app][theme]") {
    nmeasim::app::InstrumentTile tile(QStringLiteral("Depth"), QStringLiteral("m"));
    tile.enable_override(0.0, 100.0, 0.1, 1);
    CHECK_FALSE(tile.overridden());
    tile.set_override(true, 4.2);
    CHECK(tile.overridden());
    tile.set_override(false, 4.2);
    CHECK_FALSE(tile.overridden());
}

TEST_CASE("output states map to theme colours", "[app][theme]") {
    const auto& colors = theme::Theme::instance().colors();
    using State = nmeasim::io::Transport::State;
    CHECK(nmeasim::app::state_color(State::Open) == colors.ok);
    CHECK(nmeasim::app::state_color(State::Opening) == colors.warning);
    CHECK(nmeasim::app::state_color(State::Failed) == colors.danger);
    CHECK(nmeasim::app::state_color(State::Closed) == colors.inactive);
}

TEST_CASE("the main window shows its state in lights and switches themes",
          "[app][theme][integration]") {
    theme::Theme::instance().apply(theme::Mode::Night);
    nmeasim::app::MainWindow window;
    auto profile = nmeasim::io::Profile::default_profile();
    profile.outputs.clear();
    nmeasim::io::OutputConfig tcp;
    tcp.type = nmeasim::io::OutputConfig::Type::TcpServer;
    tcp.port = 0;
    tcp.bind_address = QStringLiteral("127.0.0.1");
    profile.outputs.append(tcp);
    REQUIRE(window.set_profile(profile));
    REQUIRE(window.run_led() != nullptr);
    CHECK(window.run_led()->text() == QStringLiteral("STOPPED"));
    CHECK_FALSE(window.run_led()->color().isValid());
    CHECK(window.recording_led()->isHidden());

    window.start();
    CHECK(window.run_led()->text() == QStringLiteral("RUNNING"));
    CHECK(window.run_led()->color() == theme::Theme::instance().colors().ok);
    REQUIRE(nmeasim::test::wait_until(
        [&] { return window.outputs_led()->color() == theme::Theme::instance().colors().ok; },
        5000));
    CHECK(window.outputs_led()->text() == QStringLiteral("1/1 OUTPUTS"));
    window.stop();
    CHECK(window.run_led()->text() == QStringLiteral("STOPPED"));

    // The theme menu lists the modes in the order system, night, day.
    const auto actions = window.theme_actions();
    REQUIRE(actions.size() == 3);
    CHECK(actions.at(1)->isChecked());
    actions.at(2)->trigger();
    CHECK(theme::Theme::instance().mode() == theme::Mode::Day);
    CHECK(QSettings().value(QStringLiteral("appearance/theme")).toString() ==
          QStringLiteral("day"));
    actions.at(1)->trigger();
    CHECK(theme::Theme::instance().mode() == theme::Mode::Night);
}
