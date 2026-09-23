#pragma once

#include <nmeasim/core/log/log_file.hpp>
#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/emitted_sentence.hpp>
#include <nmeasim/core/simulation/source.hpp>
#include <nmeasim/core/simulation/track_source.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

/// The replay mode: a recorded log is sent again with its original timing.
namespace nmeasim::core::simulation {

struct ReplayConfig {
    log::Log log;
    /// Values not carried by the log (for example depth in a GNSS-only log) and the clock
    /// until the first sentence with a time.
    model::VesselState seed;
    EndBehaviour end{EndBehaviour::Stop};
};

/// Replays the entries of a log as the replay clock passes their offsets.
///
/// The source provides the recorded sentences themselves rather than a state for the
/// encoders, and decodes each one into the vessel state so that the dashboard and the map
/// follow the replay. Pausing is the host's business (it stops calling `advance`); `step_once`
/// emits exactly the next entry and `seek` moves the clock, rebuilding the state from the
/// entries before the new position.
class ReplaySource final : public Source {
public:
    explicit ReplaySource(ReplayConfig config);

    const model::VesselState& advance(std::chrono::milliseconds dt) override;
    [[nodiscard]] const model::VesselState& current() const noexcept override { return state_; }
    void reset() override;
    [[nodiscard]] bool finished() const noexcept override { return finished_; }
    [[nodiscard]] std::optional<std::chrono::milliseconds> duration() const noexcept override;
    [[nodiscard]] std::chrono::milliseconds position() const noexcept override;
    void seek(std::chrono::milliseconds position) override;

    [[nodiscard]] bool provides_sentences() const noexcept override { return true; }
    std::vector<EmittedSentence> take_sentences() override;
    void step_once() override;

    /// Index of the next entry to emit; equal to `entry_count()` at the end.
    [[nodiscard]] std::size_t entry_index() const noexcept { return cursor_; }
    [[nodiscard]] std::size_t entry_count() const noexcept { return config_.log.entries.size(); }
    [[nodiscard]] const ReplayConfig& config() const noexcept { return config_; }

private:
    void emit(std::size_t index);
    void rewind();

    ReplayConfig config_;
    model::VesselState state_;
    std::vector<EmittedSentence> pending_;
    std::chrono::milliseconds clock_{0};
    std::size_t cursor_{0};
    bool finished_{false};
};

}  // namespace nmeasim::core::simulation
