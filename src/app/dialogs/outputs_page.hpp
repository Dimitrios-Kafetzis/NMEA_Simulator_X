// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The *Outputs* tab of the settings dialog: the list of a profile's outputs and an editor for
/// the selected one.
///
/// `OutputsPage` edits a working copy of `io::Profile::outputs`; `SettingsDialog` loads it
/// with `OutputsPage::load`, checks it with `OutputsPage::validate` and writes it back with
/// `OutputsPage::store` when *OK* is pressed. The fields map onto `io::OutputConfig`, whose
/// keys `docs/reference/profile.md` describes; the transports themselves are described in
/// `docs/reference/transports.md`.

#pragma once

#include <nmeasim/io/profile/profile.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QStackedWidget>
#include <QString>
#include <QWidget>

namespace nmeasim::app {

/// Settings tab with the list of outputs and an editor for the selected one.
///
/// The left column holds `list`, one row per output in the order the outputs are opened,
/// with an *Add* menu offering every `io::OutputConfig::Type` and a *Remove* button. The
/// right column shows the fields every output has (`enabled_check`, `filter_edit`,
/// `encoding_combo`, `period_spin`), then `editor_stack` with the fields of the selected
/// type, then the option group of the selected encoding (`tag_block_box`, `signalk_box` or
/// `viewsync_box`).
///
/// The page keeps its own copy of the outputs. The editor widgets show one of them at a
/// time; their values are written back into the copy (committed) when another output is
/// selected, an output is added, or `outputs`, `validate` or `store` is called. Nothing
/// reaches a profile before `store`, so discarding the page discards every change.
///
/// Every widget is a Qt child of the page and lives as long as it. The widget members are
/// public so that tests and `SettingsDialog` can drive them; code that sets them directly
/// must call `outputs` or `store` afterwards to read the result. The page declares no
/// signals or slots of its own: the *Add* menu, the *Remove* button and the list's
/// `QListWidget::currentRowChanged` signal are connected to member functions in the
/// constructor.
///
/// @see `io::OutputConfig`, `SettingsDialog`.
class OutputsPage : public QWidget {
    Q_OBJECT

public:
    /// Builds the page with every editor widget and shows the empty page.
    ///
    /// The serial port list is filled once, here, from `io::available_serial_ports`; a device
    /// plugged in later can still be typed. Call `load` to show a profile's outputs.
    ///
    /// @param parent Qt parent that owns the page; null leaves ownership to the caller.
    explicit OutputsPage(QWidget* parent = nullptr);

    /// Replaces the edited outputs with those of a profile and selects the first one.
    ///
    /// The edits of the output shown before are discarded, not committed. With no outputs
    /// the editor shows the hint to add one and the common fields are disabled.
    ///
    /// @param profile The profile whose `outputs` are copied; only that field is read.
    void load(const io::Profile& profile);
    /// Writes the edited outputs into a profile, including the values in the editor.
    ///
    /// Nothing is validated here; `SettingsDialog::accept` calls `validate` first.
    ///
    /// @param profile The profile whose `outputs` are replaced; no other field is touched.
    void store(io::Profile& profile) const;
    /// Checks that every output has the setting its transport cannot open without.
    ///
    /// Reads the output being edited from the editor. The checks, in list order: a serial
    /// output needs a port name and a baud rate (a cleared field reads as 0), a file or log
    /// output a path and a TCP client a host; values made of spaces count as missing.
    /// Everything else is either limited by its widget or accepted as it is.
    ///
    /// @return An empty string when every output is complete, otherwise a message about the
    ///     first incomplete one, numbered from 1, such as `Output 3: the serial port name is
    ///     missing.`
    [[nodiscard]] QString validate() const;

    /// Appends an output of the given type with the defaults of `io::OutputConfig` and
    /// selects it.
    ///
    /// The editor is committed first. A WebSocket server starts on port 3000, the default of
    /// Signal K servers, instead of 10110; every other type keeps the defaults.
    ///
    /// @param type The transport type of the new output.
    void add_output(io::OutputConfig::Type type);
    /// Removes the selected output and shows the one that takes its row.
    ///
    /// The edits of the removed output are dropped. Does nothing when no output is selected.
    void remove_current();
    /// Selects an output and shows it in the editor, committing the one shown before.
    ///
    /// @param index Row in `list`, from 0. -1 selects nothing and shows the empty page.
    void select(int index);
    /// Returns the number of outputs being edited.
    ///
    /// @return The size of the page's copy of the outputs, equal to the number of rows of
    ///     `list`; 0 when there are none.
    [[nodiscard]] int count() const;
    /// Returns the row of the selected output.
    ///
    /// @return The current row of `list`, from 0, or -1 when no output is selected.
    [[nodiscard]] int current_index() const;
    /// Returns the list as edited so far, including the output being edited.
    ///
    /// The output being edited is read from the editor widgets; the page's copy and `list`
    /// are not changed.
    ///
    /// @return The outputs in list order.
    [[nodiscard]] QList<io::OutputConfig> outputs() const;

    /// The outputs, one row each, titled with the type, the address, port or path, and
    /// `(Signal K)`, `(ViewSync)` or `(disabled)` when they apply.
    ///
    /// Selecting a row shows that output in the editor. The rows follow the order of
    /// `io::Profile::outputs`, which is the order the outputs are opened in.
    QListWidget* list;
    /// Edits `io::OutputConfig::enabled`; a disabled output stays in the profile but is not
    /// opened. Disabled while no output is selected.
    QCheckBox* enabled_check;
    /// Edits `io::OutputConfig::filter` as a comma-separated list; empty sends everything.
    ///
    /// For an NMEA 0183 output the entries are sentence ids, trimmed and upper-cased when
    /// committed; for a Signal K output they are path prefixes, trimmed only. Empty entries
    /// are dropped. Disabled for a ViewSync output, which ignores the filter, and while no
    /// output is selected.
    QLineEdit* filter_edit;
    /// Edits `io::OutputConfig::encoding`: NMEA 0183, Signal K, ViewSync in
    /// `io::OutputConfig::Encoding` order, so the index is the enumerator's value.
    ///
    /// Changing it shows the option group of the new encoding and enables `period_spin` and
    /// `filter_edit` as that encoding requires.
    QComboBox* encoding_combo;
    /// Edits `io::OutputConfig::period_ms`, the interval of Signal K deltas and ViewSync
    /// packets, in milliseconds; [50, 3600000] in steps of 100, the range the profile accepts.
    ///
    /// Disabled for an NMEA 0183 output, whose sentences follow their own periods.
    QSpinBox* period_spin;
    /// The fields of the selected output's type, one page per group of types.
    ///
    /// The pages, by index: 0 TCP and WebSocket server, 1 TCP client, 2 UDP, 3 serial port,
    /// 4 file and log, 5 standard output (an explanation only), 6 the hint shown when no
    /// output is selected.
    QStackedWidget* editor_stack;

    /// The *IEC 61162-450 TAG block* group, shown only for an NMEA 0183 output.
    ///
    /// @see IEC 61162-450, TAG block parameters "s" and "c".
    QGroupBox* tag_block_box;
    /// Edits `io::OutputConfig::TagBlock::enabled`: whether every sentence gets a TAG block.
    QCheckBox* tag_block_check;
    /// Edits the `s:` source identifier, `core::nmea0183::TagBlockOptions::source`; at most
    /// 15 characters, the length `core::nmea0183::sanitize_tag_source` keeps.
    ///
    /// Trimmed when committed; left empty, it stores the default `SIM0001` shown as
    /// placeholder.
    QLineEdit* tag_source_edit;
    /// Edits `core::nmea0183::TagBlockOptions::include_time`: whether the `c:` time is sent.
    QCheckBox* tag_time_check;
    /// Edits `core::nmea0183::TagBlockOptions::milliseconds`: `c:` in Unix milliseconds
    /// instead of seconds.
    QCheckBox* tag_milliseconds_check;
    /// The *Signal K* group, shown only for a Signal K output.
    QGroupBox* signalk_box;
    /// Chooses `core::signalk::SignalKOptions::context`: 0 the vessel context derived from
    /// the AIS MMSI (stored as an empty context), 1 `aircraft`, 2 the text of
    /// `signalk_context_edit`.
    ///
    /// A loaded context is shown as 0 when empty, 1 when it is exactly `aircraft` and 2
    /// otherwise.
    QComboBox* signalk_context_combo;
    /// The custom Signal K context, such as `vessels.urn:mrn:imo:mmsi:239000001`; enabled only
    /// when `signalk_context_combo` is on the custom entry.
    ///
    /// Trimmed and stored as given when committed; left empty, the output falls back to the
    /// vessel context.
    QLineEdit* signalk_context_edit;
    /// Edits `core::signalk::SignalKOptions::source_label`, sent as `source.label` in every
    /// update. Trimmed when committed; left empty, it stores the default `nmeasim`.
    QLineEdit* signalk_source_edit;
    /// The *ViewSync camera* group, shown only for a ViewSync output.
    QGroupBox* viewsync_box;
    /// Edits `core::viewsync::ViewSyncOptions::camera_altitude_m`, the camera height above the
    /// vessel in metres; [0, 100000] in whole metres.
    QDoubleSpinBox* camera_altitude_spin;
    /// Edits `core::viewsync::ViewSyncOptions::tilt_deg` in whole degrees; [0, 90], 0 looking
    /// straight down and 90 at the horizon.
    QDoubleSpinBox* tilt_spin;
    /// Edits `core::viewsync::ViewSyncOptions::roll_deg` in whole degrees; [-180, 180].
    QDoubleSpinBox* roll_spin;
    /// Chooses `core::viewsync::ViewSyncOptions::planet`: Earth (stored as empty), `sky`,
    /// `mars` or `moon`.
    ///
    /// A loaded planet that is none of these is shown, and stored again, as Earth.
    QComboBox* planet_combo;

    /// Edits `io::OutputConfig::bind_address` of a TCP or WebSocket server, trimmed when
    /// committed; `0.0.0.0` listens on every IPv4 interface.
    QLineEdit* bind_address_edit;
    /// Edits `io::OutputConfig::port` of a TCP or WebSocket server; [0, 65535], 0 lets the
    /// server pick a free port.
    QSpinBox* server_port_spin;
    /// Edits `io::OutputConfig::host` of a TCP client, trimmed when committed; must not be
    /// empty, see `validate`.
    QLineEdit* host_edit;
    /// Edits `io::OutputConfig::port` of a TCP client; [0, 65535].
    QSpinBox* client_port_spin;
    /// Edits `io::OutputConfig::reconnect_ms`, the delay before a TCP client reconnects, in
    /// milliseconds; [1, 3600000], the range the profile accepts, so a loaded value is kept.
    QSpinBox* reconnect_spin;
    /// Edits `io::UdpConfig::mode`: unicast, broadcast, multicast in `io::UdpConfig::Mode`
    /// order, so the index is the enumerator's value.
    QComboBox* udp_mode_combo;
    /// Edits `io::UdpConfig::address`, the destination IPv4 address, trimmed when committed.
    ///
    /// In broadcast mode an empty address sends to the subnet broadcast address of
    /// `udp_interface_edit`'s interface, or to `255.255.255.255` without one. The address is
    /// not checked here; one that does not parse makes the transport fail when it opens.
    QLineEdit* udp_address_edit;
    /// Edits `io::UdpConfig::port`, the destination port; [0, 65535].
    QSpinBox* udp_port_spin;
    /// Edits `io::UdpConfig::interface_name`, trimmed when committed; empty lets the
    /// operating system choose.
    QLineEdit* udp_interface_edit;
    /// Edits `io::UdpConfig::multicast_ttl` in router hops; [1, 255], the range the profile
    /// accepts, so a loaded value is kept.
    QSpinBox* udp_ttl_spin;
    /// Edits `io::SerialConfig::port_name`: an editable list of the serial ports found when
    /// the page was built, so any other device path can be typed.
    ///
    /// Trimmed when committed; must not be empty, see `validate`.
    QComboBox* serial_port_combo;
    /// Edits `io::SerialConfig::baud_rate`: an editable list of the common rates from 1200 to
    /// 115200 bit/s, accepting any positive whole number, as the profile does.
    ///
    /// A cleared field reads as 0, which `validate` rejects.
    QComboBox* baud_combo;
    /// Edits `io::SerialConfig::data_bits`: 5, 6, 7 or 8, the index plus 5.
    QComboBox* data_bits_combo;
    /// Edits `io::SerialConfig::parity`: none, even, odd, space, mark.
    QComboBox* parity_combo;
    /// Edits `io::SerialConfig::stop_bits`: 1, 1.5 or 2.
    QComboBox* stop_bits_combo;
    /// Edits `io::SerialConfig::flow_control`: none, hardware (RTS/CTS), software (XON/XOFF)
    /// in `QSerialPort::FlowControl` order, so the index is the enumerator's value.
    QComboBox* flow_control_combo;
    /// Edits `io::OutputConfig::path` of a file or log output, trimmed when committed; must not
    /// be empty, see `validate`.
    ///
    /// *Browse...* next to it opens a save dialog that does not ask for confirmation when an
    /// existing file is chosen, because choosing it changes nothing: `append_check` decides
    /// whether it is truncated. A relative path is not resolved against the profile's
    /// directory.
    QLineEdit* file_path_edit;
    /// Edits `io::OutputConfig::append`: checked appends to an existing file, unchecked
    /// truncates it when the output opens.
    QCheckBox* append_check;

private:
    /// Shows an output in the editor, committing the one shown before.
    ///
    /// Connected to `QListWidget::currentRowChanged` of `list`. Filling the widgets also
    /// updates the visible option group, `period_spin`, `filter_edit` and the enabled state
    /// of `signalk_context_edit` for the shown output. Does nothing while `load` refills the
    /// list.
    ///
    /// @param index Row of the output to show; a value outside [0, `count`) shows the empty
    ///     page, disables the common fields and hides every option group.
    void show_output(int index);
    /// Writes the editor widgets into the output being edited and refreshes the titles.
    ///
    /// Does nothing when no output is being edited.
    void commit_editor();
    /// Returns an output with the values of the editor widgets written into it.
    ///
    /// Only the fields of the output's type are written; the common fields and every encoding
    /// group are written whatever the encoding. Nothing of the page changes.
    ///
    /// @param output The output the editor shows, as last committed; taken by value.
    /// @return `output` with the edited values.
    [[nodiscard]] io::OutputConfig edited(io::OutputConfig output) const;
    /// Shows the option group of the encoding selected in `encoding_combo`, hides the other
    /// two, and enables `period_spin` and `filter_edit` for it.
    ///
    /// Connected to `QComboBox::currentIndexChanged` of `encoding_combo`.
    void update_encoding_widgets();
    /// Sets the text of every row of `list` from `title_of` of its output.
    void refresh_titles();
    /// Returns the title of an output in `list`.
    ///
    /// @param output The output to describe.
    /// @return The type name, then ` - ` and the address and port, the serial port and baud
    ///     rate, or the path when that is not blank, then `(Signal K)` or `(ViewSync)` for
    ///     those encodings and `(disabled)` for a disabled output.
    [[nodiscard]] static QString title_of(const io::OutputConfig& output);
    /// Returns the `editor_stack` page that edits a transport type.
    ///
    /// @param type The transport type.
    /// @return The page index from 0 to 5 listed at `editor_stack`; 6, the empty page, for a
    ///     value outside the enumeration.
    [[nodiscard]] static int page_of(io::OutputConfig::Type type);

    /// The outputs being edited, in list order; row `n` of `list` shows element `n`.
    QList<io::OutputConfig> outputs_;
    /// Index in `outputs_` of the output the editor widgets show, or -1 when they show none
    /// and must not be committed.
    int editing_{-1};
    /// True while `load` refills `list`, so that the row changes it causes do not show or
    /// commit an output.
    bool loading_{false};
    /// Directory of the loaded profile's file, `io::Profile::base_directory`, against which
    /// the file dialog of the *Browse...* button resolves a relative output path; empty for a
    /// profile without a file.
    QString profile_directory_;
};

}  // namespace nmeasim::app
