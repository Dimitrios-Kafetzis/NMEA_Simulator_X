// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The `SimulationRunner`, which drives a simulation from a real timer and fans its output
/// out to transports, and the `OutputChannel` it keeps per output.
///
/// The runner lives on the thread that runs the Qt event loop. Hosts (the command-line tool
/// and the desktop application) observe it through signals and read the vessel state through
/// `SimulationRunner::simulation` between ticks. Each output chooses an encoding, as
/// ADR 0014 decides: NMEA 0183 sentences, optionally framed with IEC 61162-450 TAG blocks,
/// Signal K deltas or ViewSync packets.
///
/// @see docs/explanation/architecture.md, section "Runtime model".
/// @see docs/adr/0014-multi-encoding-outputs.md

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

namespace nmeasim::io {

class LogTransport;

/// One enabled output of a running profile: its transport, its filter and its counters.
///
/// The channel owns its transport. It is move-only because of that `std::unique_ptr`. The
/// runner builds one channel per enabled output in `SimulationRunner::apply_profile` and
/// keeps it, counters included, until the next profile is applied.
struct OutputChannel {
    /// The profile output this channel was built from; `encoding` decides what it carries.
    OutputConfig config;
    /// The transport built from `config`; never null in a channel built by the runner.
    std::unique_ptr<Transport> transport;
    /// Entries of the output's `filter`; empty admits everything.
    ///
    /// For an NMEA 0183 channel the entries are registry or custom sentence ids, for a
    /// Signal K channel they are path prefixes, and a ViewSync channel ignores them.
    QSet<QString> filter;
    /// Lines written to the transport since the profile was applied: sentences, Signal K
    /// deltas or ViewSync packets.
    ///
    /// Counts only lines handed to an open transport, and keeps counting across stop and
    /// start.
    qint64 sentences_sent{0};
    /// Simulated time, as `core::simulation::Simulation::elapsed` counts it, at which the
    /// next state message of a Signal K or ViewSync channel is due.
    ///
    /// Set to the current elapsed time by `SimulationRunner::start`, so the first message goes
    /// out on the first tick, then advanced by `period_ms` with
    /// `core::simulation::next_due_after`. Unused by NMEA 0183 channels.
    std::chrono::milliseconds next_due{0};
    /// Packet counter of a ViewSync channel, sent as the first field of the next packet.
    ///
    /// Reset to zero by `SimulationRunner::start` and incremented after every packet;
    /// unused by other encodings.
    quint32 counter{0};

    /// Returns whether the filter admits an NMEA 0183 sentence.
    ///
    /// @param id Registry or custom sentence id, such as `RMC` or `BARO`, compared exactly
    ///   (case-sensitive) with the filter entries.
    /// @return True when the filter is empty or contains `id`.
    [[nodiscard]] bool admits(const QString& id) const {
        return filter.isEmpty() || filter.contains(id);
    }
    /// Returns whether the filter admits a Signal K path.
    ///
    /// The comparison is a case-insensitive string prefix match, not aligned to the dots
    /// between path segments: the entry `navigation.speed` also admits
    /// `navigation.speedThroughWater`.
    ///
    /// @param path Full Signal K path, such as `environment.wind.speedApparent`.
    /// @return True when the filter is empty or `path` starts with one of its entries.
    [[nodiscard]] bool admits_path(const QString& path) const;
    /// Returns whether this channel carries NMEA 0183 sentences.
    ///
    /// @return True for the NMEA 0183 encoding; false for Signal K and ViewSync, which carry
    ///   state messages instead.
    [[nodiscard]] bool carries_sentences() const noexcept {
        return config.encoding == OutputConfig::Encoding::Nmea0183;
    }
};

/// Runs the simulation of a profile on a timer and writes its output to the profile's
/// transports and to an optional recording.
///
/// Life cycle: `apply_profile` builds the simulation and one `OutputChannel` per enabled
/// output, with every transport closed. `start` opens the outputs and starts the tick timer;
/// `pause` and `resume` stop and continue the simulated clock while the outputs stay open;
/// `stop` stops the timer and closes the outputs. `step` and `seek` work in any of these
/// states once a profile is applied. A finite source that reaches its end emits `finished`
/// and stops the run.
///
/// Timing: the tick timer fires every `Profile::tick_ms` milliseconds of wall-clock time. Each
/// tick hands the simulation the wall-clock time elapsed since the previous tick in whole
/// milliseconds, carrying the remainder to the next tick so that the simulated clock keeps
/// pace with the real one. When the host falls behind by more than one second (the event
/// loop was blocked, the machine was suspended), the tick advances one second only and the
/// rest is dropped, so the vessel never jumps ahead by more than one second in a tick. Time
/// spent paused is not caught up either.
///
/// Encoding: every sentence the simulation emits goes, as a line ending in CR LF, to every
/// open NMEA 0183 channel whose filter admits it, with an IEC 61162-450 TAG block in front
/// when the output enables one. After every tick, each open Signal K and ViewSync channel
/// that is due gets one message built from the current state. A recording receives every
/// sentence, unfiltered and without TAG block, but no state message.
///
/// Ownership: the runner owns the simulation, the channels with their transports and the
/// recording transport. The transports have no Qt parent.
///
/// Threads: the runner, its simulation and its transports are used from the thread that
/// created the runner, which must run a Qt event loop for the timer and the sockets. Every
/// signal is emitted on that thread, either synchronously from inside the public call that
/// caused it or from a timer or socket event.
///
/// @see docs/explanation/architecture.md, section "Runtime model".
class SimulationRunner : public QObject {
    Q_OBJECT

public:
    /// Creates a runner without a simulation.
    ///
    /// Call `apply_profile` before `start`; until then `start`, `step` and `seek` do nothing.
    ///
    /// @param parent Qt parent that owns the runner; may be null.
    explicit SimulationRunner(QObject* parent = nullptr);
    /// Destroys the runner, stopping a running simulation first.
    ///
    /// Emits `stopped` from the destructor when the run was going.
    ~SimulationRunner() override;

    /// Replaces the simulation and the outputs with those described by a profile.
    ///
    /// Stops a running simulation first (emitting `stopped`), then builds the source (delta,
    /// track or replay) with the profile's seed, the sentence schedule, and one channel per
    /// enabled output, in profile order. The transports are created closed; they open at
    /// `start`. The seed's clock is set to `Profile::start_time`, or to the current UTC time
    /// when that is empty; a timed track then follows its timestamps and a replay the time
    /// fields of its sentences. The count of emitted sentences is reset to zero; a recording
    /// is kept and takes the new profile name.
    ///
    /// Every transport is built once, here. A Signal K WebSocket output gets a greeting
    /// function that builds the Signal K hello from the current state and wall clock each
    /// time a client connects. Transports that later fail to open are not a profile error:
    /// they are reported through `output_error` when the run starts.
    ///
    /// @param profile The profile to run; copied, so it need not outlive the call.
    /// @param[out] error Receives the reason when the profile cannot be applied: a track or
    ///   log file that cannot be read or parsed, or an enabled output of a type the runner
    ///   cannot build. May be null. Untouched on success.
    /// @return True when the profile was applied. False when it could not be; the run is then
    ///   stopped but the previous simulation, outputs and profile are kept.
    bool apply_profile(const Profile& profile, QString* error);

    /// Opens every output and the recording, and starts ticking.
    ///
    /// Does nothing before a profile has been applied or while running. Outputs that fail to
    /// open are reported through `output_error`, synchronously from this call, and skipped;
    /// the run proceeds with the rest. ViewSync counters restart at zero and every state
    /// message is due at the first tick. The simulation itself continues from where it was:
    /// starting again after `stop` does not rewind it.
    ///
    /// Emits `started`.
    void start();
    /// Stops advancing the simulated clock while keeping the outputs open.
    ///
    /// Does nothing when not running or already paused. The timer keeps running but its ticks
    /// are ignored, so nothing is sent until `resume` or `step`. Emits `paused_changed` with
    /// `true`.
    void pause();
    /// Continues a paused run without catching up on the time spent paused.
    ///
    /// Does nothing when not running or not paused. Emits `paused_changed` with `false`.
    void resume();
    /// Stops ticking and closes every output and the recording.
    ///
    /// Safe to call in any state. Clears the paused flag, emitting `paused_changed` with
    /// `false` when the run was paused. The simulation keeps its state and the channels keep
    /// their counters. Emits `stopped` only when the run was going, after `paused_changed`.
    void stop();

    /// Takes the smallest step while paused: one recorded sentence during a replay, one tick
    /// of `Profile::tick_ms` otherwise.
    ///
    /// Does nothing before a profile has been applied. Starts the run paused when it is not
    /// running (emitting `started` and `paused_changed`) and pauses it when it is running
    /// (emitting `paused_changed` unless already paused). Then emits what the step produced
    /// through the outputs and `sentence_emitted`, emits `ticked`, and emits `finished` and
    /// `stopped` when the step reached the end of a finite source.
    void step();
    /// Moves a finite source to a position; the state changes at once.
    ///
    /// Endless sources (the delta simulation) keep their position, but in either case every
    /// NMEA 0183 sentence and every Signal K and ViewSync message becomes due again at the
    /// next tick, so that receivers see the new position at once. Nothing is sent by the seek
    /// itself. Works whether
    /// running, paused or stopped. The wall-clock reference restarts, so the time since the
    /// previous tick is not handed to the simulation. Does nothing before a profile has been
    /// applied. Emits `ticked`.
    ///
    /// @param position Position within the source, clamped by the source to
    ///   [0, `duration()`].
    void seek(std::chrono::milliseconds position);
    /// Returns the length of a finite source.
    ///
    /// @return The duration of the track or log in simulated time; `std::nullopt` for the
    ///   delta simulation and before a profile has been applied.
    [[nodiscard]] std::optional<std::chrono::milliseconds> duration() const;
    /// Returns the elapsed position within a finite source.
    ///
    /// @return The position in [0, `duration()`], restarting from zero when a looping source
    ///   wraps; zero for the delta simulation and before a profile has been applied.
    [[nodiscard]] std::chrono::milliseconds position() const;

    /// Records every emitted sentence to a log file, in addition to the profile outputs.
    ///
    /// The recording uses the log format of ADR 0012 and holds the plain sentences, before any
    /// filter and without TAG block; Signal K and ViewSync messages are not recorded. The file
    /// is truncated when the recording first opens and continued across stop and start until
    /// the recording is cleared or replaced. While running the file opens at once, and the
    /// previous recording, if any, is closed and replaced only once it has; otherwise the
    /// recording is set at once and the file opens at the next `start`, where a failure is
    /// reported through `output_error` and leaves the recording set.
    ///
    /// Emits `recording_changed` with `path` when the recording was set or cleared. When the
    /// file cannot be opened at once, emits `output_error` synchronously instead and changes
    /// nothing.
    ///
    /// @param path Log file to record to; an empty path stops recording.
    /// @return False when the file was opened at once and that failed; the previous recording,
    ///   or none, then stays. True otherwise.
    /// @see docs/reference/log-format.md
    bool set_recording(const QString& path);
    /// Returns the path of the current recording.
    ///
    /// @return The path given to `set_recording`; empty when not recording.
    [[nodiscard]] QString recording_path() const;
    /// Returns whether a recording is set, whether or not the run is going.
    ///
    /// @return True from a successful `set_recording` call with a non-empty path until one
    ///   with an empty path, also when the file failed to open at a later `start`.
    [[nodiscard]] bool is_recording() const noexcept { return recorder_ != nullptr; }
    /// Returns the log transport of the recording, for its state and counters.
    ///
    /// @return The recording transport, owned by the runner and valid until the next
    ///   `set_recording` call or the runner's destruction; null when not recording.
    [[nodiscard]] const LogTransport* recorder() const noexcept { return recorder_.get(); }

    /// Returns whether the run is going.
    ///
    /// @return True between `start` and `stop`, paused or not.
    [[nodiscard]] bool is_running() const noexcept { return tick_timer_.isActive(); }
    /// Returns whether a running simulation is paused.
    ///
    /// @return True from `pause` or `step` until `resume` or `stop`; always false when not
    ///   running.
    [[nodiscard]] bool is_paused() const noexcept { return paused_; }

    /// Returns the current simulation, for reading the state and changing it between ticks.
    ///
    /// @return The simulation owned by the runner, valid until the next successful
    ///   `apply_profile` or the runner's destruction; null before a profile has been applied.
    [[nodiscard]] core::simulation::Simulation* simulation() noexcept { return simulation_.get(); }
    /// Returns the current simulation, read-only.
    ///
    /// @return The simulation owned by the runner, valid until the next successful
    ///   `apply_profile` or the runner's destruction; null before a profile has been applied.
    [[nodiscard]] const core::simulation::Simulation* simulation() const noexcept {
        return simulation_.get();
    }
    /// Returns the channels of the enabled outputs of the applied profile.
    ///
    /// @return The channels in profile order, disabled outputs omitted; empty before a profile
    ///   has been applied. The reference stays valid for the runner's lifetime; the elements
    ///   are replaced by the next successful `apply_profile`.
    [[nodiscard]] const std::vector<OutputChannel>& outputs() const noexcept { return outputs_; }
    /// Returns the profile last applied successfully.
    ///
    /// @return The runner's copy, replaced by the next successful `apply_profile`; a
    ///   default-constructed profile before the first one.
    [[nodiscard]] const Profile& profile() const noexcept { return profile_; }
    /// Returns the number of lines produced since the profile was applied.
    ///
    /// @return Every sentence the simulation emitted, before filtering and whether or not an
    ///   output was open, plus every Signal K or ViewSync message sent, counted once per
    ///   channel. Equal to the number of `sentence_emitted` signals since the profile was
    ///   applied; kept across stop and start.
    [[nodiscard]] qint64 sentences_emitted() const noexcept { return sentences_emitted_; }

signals:
    /// Emitted when the outputs have been opened and ticking began, from `start` or from
    /// `step` on a runner that was not running.
    void started();
    /// Emitted when the run is paused or resumed, from `pause`, `resume` or `step`, and with
    /// `false` when `stop` ends a paused run.
    ///
    /// Not emitted when the flag does not change.
    ///
    /// @param paused True when the run was paused, false when it was resumed or stopped.
    void paused_changed(bool paused);
    /// Emitted when a running simulation stopped and its outputs were closed, from `stop`,
    /// `apply_profile`, the destructor, or after `finished` at the end of a finite source.
    void stopped();
    /// Emitted after every tick that advanced the simulation, every `step` and every `seek`.
    ///
    /// Hosts refresh their view of `simulation()->state()` in response. Ticks that are ignored
    /// (while paused, or less than a millisecond after the previous one) do not emit it.
    void ticked();
    /// Emitted for every line produced, before filtering and whether or not an output is
    /// open, from a tick or from `step`.
    ///
    /// NMEA 0183 sentences are reported once each. Signal K and ViewSync messages are reported
    /// once per channel that sent one.
    ///
    /// @param id The registry or custom sentence id (for a replayed sentence, its formatter,
    ///   such as `RMC`), or `SIGNALK` or `VIEWSYNC` for a state message.
    /// @param text The sentence or message without TAG block and line terminator.
    void sentence_emitted(const QString& id, const QString& text);
    /// Emitted when an output or the recording reports an error through
    /// `Transport::error_occurred`.
    ///
    /// Emitted synchronously from `start` or `set_recording` when a transport fails to open,
    /// and later from socket, device or write errors.
    ///
    /// @param description The `Transport::description` of the transport, naming it.
    /// @param message The error message of the transport.
    void output_error(const QString& description, const QString& message);
    /// Emitted when the track or log reached its end and does not loop, from a tick or from
    /// `step`; `stopped` follows immediately.
    void finished();
    /// Emitted when `set_recording` sets or clears the recording; not when it fails to open
    /// the file at once.
    ///
    /// @param path The new recording path, or empty when the recording was cleared.
    void recording_changed(const QString& path);

private:
    /// Advances the simulation by the wall-clock time since the previous tick and sends what
    /// it produced; connected to the timeout of `tick_timer_`.
    ///
    /// Emits `sentence_emitted`, `ticked`, and `finished` and `stopped` at the end of a finite
    /// source. Returns without effect while paused or when less than a millisecond has passed.
    void tick();
    /// Writes sentences to the admitting NMEA 0183 channels and to the recording.
    ///
    /// The TAG block, where enabled, is put in front of the sentence by
    /// `core::nmea0183::prepend_tag_block` and carries the simulated UTC time of the current
    /// state as its `c:` parameter. Emits `sentence_emitted` for every sentence.
    ///
    /// @param sentences The sentences the simulation produced, in order, without line
    ///   terminator.
    /// @see IEC 61162-450, TAG block parameter "c".
    void emit_sentences(const std::vector<core::simulation::EmittedSentence>& sentences);
    /// Sends one message on every open Signal K and ViewSync channel that is due.
    ///
    /// Signal K channels send a delta with the paths their filter admits; ViewSync channels
    /// send a packet with their counter. Emits `sentence_emitted` for every message.
    ///
    /// @see https://signalk.org/specification/1.7.0/doc/data_model.html
    void emit_state_messages();
    /// Emits `finished` and stops the run (emitting `stopped`) when the source has reached
    /// its end.
    void finish_if_done();
    /// Restarts the wall-clock reference of the tick, so that the time before this call is
    /// never handed to the simulation.
    void restart_wall_clock();
    /// Creates the transport for an output, closed.
    ///
    /// @param config The output to build the transport for.
    /// @return A new transport without a Qt parent; null for an output type the runner does
    ///   not know.
    std::unique_ptr<Transport> make_transport(const OutputConfig& config) const;
    /// Creates the source of a profile's mode, seeded with the profile's start time.
    ///
    /// @param profile The profile whose mode, seed, track or replay settings to use.
    /// @param[out] error Receives the reason when a track or log cannot be loaded; may be
    ///   null.
    /// @return The new source; null when the track or log cannot be loaded.
    std::unique_ptr<core::simulation::Source> make_source(const Profile& profile,
                                                          QString* error) const;

    /// The profile last applied successfully.
    Profile profile_;
    /// The simulation of `profile_`; null before the first successful `apply_profile`.
    std::unique_ptr<core::simulation::Simulation> simulation_;
    /// One channel per enabled output of `profile_`, in profile order.
    std::vector<OutputChannel> outputs_;
    /// The recording, or null when not recording.
    std::unique_ptr<LogTransport> recorder_;
    /// Precise timer firing every `Profile::tick_ms` milliseconds while running; active from
    /// `start` to `stop`, paused or not.
    QTimer tick_timer_;
    /// Wall-clock reference of the tick, restarted by `start`, `resume` and `seek`.
    QElapsedTimer wall_clock_;
    /// Wall-clock time since `wall_clock_` started that has been handed to the simulation.
    std::chrono::nanoseconds wall_consumed_{0};
    /// True while a running simulation is paused.
    bool paused_{false};
    /// Lines produced since the profile was applied, as `sentences_emitted` returns.
    qint64 sentences_emitted_{0};
};

}  // namespace nmeasim::io
