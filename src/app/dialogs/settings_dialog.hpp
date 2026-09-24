// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The settings dialog, which edits a copy of a profile in four tabs.
///
/// `MainWindow` opens a `SettingsDialog` from *File > Settings...* and, when it is accepted,
/// makes `SettingsDialog::profile` the current profile. The tabs are `SimulationPage`,
/// `VesselPage`, `SentencesPage` and `OutputsPage`; `docs/reference/desktop-app.md` describes
/// them for the user.

#pragma once

#include <nmeasim/io/profile/profile.hpp>

#include <QDialog>
#include <QLabel>
#include <QTabWidget>

namespace nmeasim::app {

class OutputsPage;
class SentencesPage;
class SimulationPage;
class VesselPage;

/// Modal dialog that edits a copy of a profile in four tabs and applies the edits on *OK*.
///
/// The tabs are *Simulation*, *Vessel*, *Sentences* and *Outputs*, each a page that loads
/// its part of the profile in the constructor. *OK* runs `accept`, which validates the pages
/// in tab order and either shows the first problem under the tabs, leaving the dialog open,
/// or stores every page into `profile` and closes the dialog. *Cancel*, Escape and closing
/// the window reject the dialog and leave `profile` as it was passed in. The dialog never
/// touches a profile file or the running simulation; the caller decides what to do with the
/// result.
///
/// The dialog owns its copy of the profile and, through Qt parenthood, the tab widget, the
/// four pages and the error label.
///
/// @see `MainWindow`, `io::Profile`.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    /// Builds the dialog and loads a copy of a profile into every page.
    ///
    /// The window is titled `Settings - ` followed by the profile name, is modal and opens at
    /// 900 by 640 pixels. It is not shown; call `exec` or `open`.
    ///
    /// @param profile The profile to edit; it is copied, so the caller's object is never
    ///     changed and need not outlive the dialog.
    /// @param parent Qt parent that owns the dialog and that it is centred on; null leaves
    ///     ownership to the caller.
    explicit SettingsDialog(const io::Profile& profile, QWidget* parent = nullptr);

    /// Returns the dialog's profile.
    ///
    /// @return The profile as passed to the constructor until `accept` succeeds, then the
    ///     edited profile. The reference stays valid for the lifetime of the dialog and its
    ///     value changes only in a successful `accept`.
    [[nodiscard]] const io::Profile& profile() const noexcept { return profile_; }

    /// Returns the *Simulation* tab: mode, track and replay, clock, seed values, drift,
    /// destination and steering.
    ///
    /// @return The page, owned by the dialog.
    [[nodiscard]] SimulationPage* simulation_page() const noexcept { return simulation_; }
    /// Returns the *Vessel* tab: engines and AIS static data.
    ///
    /// @return The page, owned by the dialog.
    [[nodiscard]] VesselPage* vessel_page() const noexcept { return vessel_; }
    /// Returns the *Sentences* tab: registry sentence settings and custom sentences.
    ///
    /// @return The page, owned by the dialog.
    [[nodiscard]] SentencesPage* sentences_page() const noexcept { return sentences_; }
    /// Returns the *Outputs* tab.
    ///
    /// @return The page, owned by the dialog.
    [[nodiscard]] OutputsPage* outputs_page() const noexcept { return outputs_; }
    /// Returns the tab widget, whose current tab `accept` switches to the page with a
    /// problem.
    ///
    /// @return The tab widget, owned by the dialog.
    [[nodiscard]] QTabWidget* tabs() const noexcept { return tabs_; }
    /// Returns the validation message shown under the tabs.
    ///
    /// @return The message of the last refused `accept`; empty before the first `accept`
    ///     and after one that succeeded.
    [[nodiscard]] QString error_text() const;

public slots:
    /// Validates every page and, when all pass, stores them into the profile and accepts the
    /// dialog.
    ///
    /// Connected to the *OK* button. The pages are checked in tab order: simulation, vessel,
    /// sentences, outputs. At the first problem its message is shown under the tabs, that
    /// page becomes the current tab and the dialog stays open with `profile` unchanged.
    /// Otherwise the message is hidden, every page is stored into `profile`, and
    /// `QDialog::accept` sets the result to `QDialog::Accepted`, hides the dialog and emits
    /// `accepted` and `finished`.
    ///
    /// @note Validating the outputs page commits the output being edited into that page's
    ///     copy, even when the dialog is then refused.
    void accept() override;

private:
    /// The edited copy: the constructor's profile until `accept` succeeds, the stored result
    /// after.
    io::Profile profile_;
    /// The four tabs, in the order simulation, vessel, sentences, outputs.
    QTabWidget* tabs_;
    /// The *Simulation* tab, owned through Qt parenthood.
    SimulationPage* simulation_;
    /// The *Vessel* tab, owned through Qt parenthood.
    VesselPage* vessel_;
    /// The *Sentences* tab, owned through Qt parenthood.
    SentencesPage* sentences_;
    /// The *Outputs* tab, owned through Qt parenthood.
    OutputsPage* outputs_;
    /// Word-wrapped line under the tabs with the reason the last `accept` was refused, drawn
    /// in the palette's highlight colour; hidden when there is none.
    QLabel* error_label_;
};

}  // namespace nmeasim::app
