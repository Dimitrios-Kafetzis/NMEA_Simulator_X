#pragma once

#include <nmeasim/core/simulation/simulation.hpp>
#include <nmeasim/io/profile/profile.hpp>
#include <nmeasim/io/transport.hpp>

#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <memory>
#include <vector>

/// Drives a simulation from real timers and fans its sentences out to transports.
///
/// The runner lives on the Qt event loop thread. Hosts observe it through signals and read
/// the vessel state through `simulation()` between ticks.
namespace nmeasim::io {

/// A transport together with the sentence filter configured for it.
struct OutputChannel {
    OutputConfig config;
    std::unique_ptr<Transport> transport;
    /// Registry ids admitted by this channel; empty admits everything.
    QSet<QString> filter;
    qint64 sentences_sent{0};

    [[nodiscard]] bool admits(const QString& id) const {
        return filter.isEmpty() || filter.contains(id);
    }
};

class SimulationRunner : public QObject {
    Q_OBJECT

public:
    explicit SimulationRunner(QObject* parent = nullptr);
    ~SimulationRunner() override;

    /// Replaces the simulation and the outputs with those described by `profile`. Stops a
    /// running simulation first. Returns false and sets `error` when the profile cannot be
    /// applied; transports that fail to open are reported through `output_error` instead.
    bool apply_profile(const Profile& profile, QString* error);

    /// Opens every enabled output and starts ticking. Outputs that fail to open are reported
    /// and skipped; the run proceeds with the rest.
    void start();
    void pause();
    void resume();
    /// Stops ticking and closes every output.
    void stop();

    [[nodiscard]] bool is_running() const noexcept { return tick_timer_.isActive(); }
    [[nodiscard]] bool is_paused() const noexcept { return paused_; }

    [[nodiscard]] core::simulation::Simulation* simulation() noexcept { return simulation_.get(); }
    [[nodiscard]] const core::simulation::Simulation* simulation() const noexcept {
        return simulation_.get();
    }
    [[nodiscard]] const std::vector<OutputChannel>& outputs() const noexcept { return outputs_; }
    [[nodiscard]] const Profile& profile() const noexcept { return profile_; }
    [[nodiscard]] qint64 sentences_emitted() const noexcept { return sentences_emitted_; }

signals:
    void started();
    void paused_changed(bool paused);
    void stopped();
    /// Emitted after every tick; hosts refresh their view of `simulation()->state()`.
    void ticked();
    /// Every sentence produced, before filtering, with its registry id.
    void sentence_emitted(const QString& id, const QString& text);
    void output_error(const QString& description, const QString& message);

private:
    void tick();
    std::unique_ptr<Transport> make_transport(const OutputConfig& config) const;

    Profile profile_;
    std::unique_ptr<core::simulation::Simulation> simulation_;
    std::vector<OutputChannel> outputs_;
    QTimer tick_timer_;
    QElapsedTimer wall_clock_;
    bool paused_{false};
    qint64 sentences_emitted_{0};
};

}  // namespace nmeasim::io
