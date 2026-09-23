#pragma once

#include <nmeasim/core/simulation/emitted_sentence.hpp>
#include <nmeasim/core/simulation/simulation.hpp>
#include <nmeasim/io/profile/profile.hpp>
#include <nmeasim/io/transport.hpp>

#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

/// Drives a simulation from real timers and fans its sentences out to transports.
///
/// The runner lives on the Qt event loop thread. Hosts observe it through signals and read
/// the vessel state through `simulation()` between ticks.
namespace nmeasim::io {

class LogTransport;

/// A transport together with the sentence filter configured for it.
struct OutputChannel {
    /// The profile output this channel was built from.
    OutputConfig config;
    /// The transport built from `config`.
    std::unique_ptr<Transport> transport;
    /// Registry ids admitted by this channel; empty admits everything. For a Signal K
    /// channel the entries are path prefixes.
    QSet<QString> filter;
    /// Lines written to the transport so far: sentences, Signal K deltas or ViewSync packets.
    qint64 sentences_sent{0};
    /// Simulated time at which the next state message (Signal K, ViewSync) is due.
    std::chrono::milliseconds next_due{0};
    /// Packets sent so far on a ViewSync channel.
    quint32 counter{0};

    /// True when the filter admits the registry or custom sentence `id`.
    [[nodiscard]] bool admits(const QString& id) const {
        return filter.isEmpty() || filter.contains(id);
    }
    /// True for a Signal K path admitted by the filter: empty admits everything, otherwise
    /// the path must start with one of the entries.
    [[nodiscard]] bool admits_path(const QString& path) const;
    /// True when this channel carries NMEA 0183 sentences.
    [[nodiscard]] bool carries_sentences() const noexcept {
        return config.encoding == OutputConfig::Encoding::Nmea0183;
    }
};

/// Runs the simulation of a profile on a timer and writes its output to the profile's
/// transports.
class SimulationRunner : public QObject {
    Q_OBJECT

public:
    /// Creates a runner without a simulation; call `apply_profile` before `start`.
    explicit SimulationRunner(QObject* parent = nullptr);
    ~SimulationRunner() override;

    /// Replaces the simulation and the outputs with those described by `profile`. Stops a
    /// running simulation first. Returns false and sets `error` when the profile cannot be
    /// applied, for example when its track or log file cannot be read; transports that fail
    /// to open are reported through `output_error` instead.
    bool apply_profile(const Profile& profile, QString* error);

    /// Opens every enabled output and starts ticking. Outputs that fail to open are reported
    /// and skipped; the run proceeds with the rest.
    void start();
    /// Stops advancing the simulation while keeping the outputs open.
    void pause();
    /// Continues a paused run without catching up on the time spent paused.
    void resume();
    /// Stops ticking and closes every output.
    void stop();

    /// Takes the smallest step while paused: one recorded sentence during a replay, one
    /// tick otherwise. Starts the run paused when it is not running, pauses it when it is.
    void step();
    /// Moves a finite source (a track or a log) to `position`; the state changes at once.
    /// Endless sources ignore it.
    void seek(std::chrono::milliseconds position);
    /// Length of a finite source; nullopt for the delta simulation.
    [[nodiscard]] std::optional<std::chrono::milliseconds> duration() const;
    /// Elapsed position within a finite source.
    [[nodiscard]] std::chrono::milliseconds position() const;

    /// Records every emitted sentence, in addition to the profile outputs, to a log file
    /// (ADR 0012). The file is truncated when the recording starts and continued across
    /// stop and start until the recording is cleared with an empty path. Returns false and
    /// reports through `output_error` when the file cannot be opened.
    bool set_recording(const QString& path);
    /// Path of the current recording; empty when not recording.
    [[nodiscard]] QString recording_path() const;
    /// True while a recording path is set, whether or not the run is going.
    [[nodiscard]] bool is_recording() const noexcept { return recorder_ != nullptr; }
    /// The log transport of the recording, or null when not recording.
    [[nodiscard]] const LogTransport* recorder() const noexcept { return recorder_.get(); }

    /// True between `start` and `stop`, paused or not.
    [[nodiscard]] bool is_running() const noexcept { return tick_timer_.isActive(); }
    /// True while a running simulation is paused.
    [[nodiscard]] bool is_paused() const noexcept { return paused_; }

    /// The current simulation, or null before a profile has been applied.
    [[nodiscard]] core::simulation::Simulation* simulation() noexcept { return simulation_.get(); }
    /// The current simulation, or null before a profile has been applied.
    [[nodiscard]] const core::simulation::Simulation* simulation() const noexcept {
        return simulation_.get();
    }
    /// The channels of the enabled outputs of the applied profile, in profile order.
    [[nodiscard]] const std::vector<OutputChannel>& outputs() const noexcept { return outputs_; }
    /// The profile last applied successfully.
    [[nodiscard]] const Profile& profile() const noexcept { return profile_; }
    /// Sentences and state messages produced since the profile was applied, before filtering.
    [[nodiscard]] qint64 sentences_emitted() const noexcept { return sentences_emitted_; }

signals:
    /// The outputs were opened and ticking began.
    void started();
    /// The run was paused (`paused` true) or resumed.
    void paused_changed(bool paused);
    /// A running simulation stopped and its outputs were closed.
    void stopped();
    /// Emitted after every tick, step and seek; hosts refresh their view of
    /// `simulation()->state()`.
    void ticked();
    /// Every sentence produced, before filtering, with its registry id (or the formatter of
    /// a replayed sentence). Signal K and ViewSync messages are reported with the ids
    /// `SIGNALK` and `VIEWSYNC`.
    void sentence_emitted(const QString& id, const QString& text);
    /// An output or the recording reported an error; `description` names the transport.
    void output_error(const QString& description, const QString& message);
    /// The track or log reached its end; `stopped` follows.
    void finished();
    /// The recording was started (non-empty path) or cleared (empty path).
    void recording_changed(const QString& path);

private:
    void tick();
    void emit_sentences(const std::vector<core::simulation::EmittedSentence>& sentences);
    /// Sends the state messages of the Signal K and ViewSync channels that are due.
    void emit_state_messages();
    void finish_if_done();
    void refresh_greetings();
    std::unique_ptr<Transport> make_transport(const OutputConfig& config) const;
    std::unique_ptr<core::simulation::Source> make_source(const Profile& profile,
                                                          QString* error) const;

    Profile profile_;
    std::unique_ptr<core::simulation::Simulation> simulation_;
    std::vector<OutputChannel> outputs_;
    std::unique_ptr<LogTransport> recorder_;
    QTimer tick_timer_;
    QElapsedTimer wall_clock_;
    bool paused_{false};
    qint64 sentences_emitted_{0};
};

}  // namespace nmeasim::io
