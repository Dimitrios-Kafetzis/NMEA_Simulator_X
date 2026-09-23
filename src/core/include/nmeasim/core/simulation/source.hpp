#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/simulation/emitted_sentence.hpp>

#include <chrono>
#include <optional>
#include <vector>

/// A source produces the vessel state for each simulation tick.
namespace nmeasim::core::simulation {

class Source {
public:
    virtual ~Source() = default;

    /// Advances the simulation by `dt` and returns the state at the new time.
    virtual const model::VesselState& advance(std::chrono::milliseconds dt) = 0;

    /// The most recently produced state.
    [[nodiscard]] virtual const model::VesselState& current() const noexcept = 0;

    /// Returns the source to its initial state.
    virtual void reset() = 0;

    /// True once a finite source (a track, a log) has nothing more to produce.
    [[nodiscard]] virtual bool finished() const noexcept { return false; }

    /// Length of a finite source in simulated time; nullopt for an endless one.
    [[nodiscard]] virtual std::optional<std::chrono::milliseconds> duration() const noexcept {
        return std::nullopt;
    }

    /// Elapsed position within a finite source, between zero and `duration()`. Endless
    /// sources report zero.
    [[nodiscard]] virtual std::chrono::milliseconds position() const noexcept { return {}; }

    /// Moves a finite source to `position`, clamped to its duration. Endless sources ignore
    /// it. The state reflects the new position immediately, without a call to `advance`.
    virtual void seek(std::chrono::milliseconds /*position*/) {}

    /// True for a source that replays recorded sentences instead of producing a state for
    /// the encoders. The simulation then returns `take_sentences()` from every step and
    /// leaves the sentence schedule idle.
    [[nodiscard]] virtual bool provides_sentences() const noexcept { return false; }

    /// The sentences that became due during the last `advance` or `step_once`, in order.
    /// Empty for sources that do not provide sentences.
    virtual std::vector<EmittedSentence> take_sentences() { return {}; }

    /// Advances a sentence-providing source by exactly one recorded sentence, moving its
    /// clock to that sentence. Other sources ignore it.
    virtual void step_once() {}

    /// Sets or clears the destination the autopilot sentences describe. Sources that decode
    /// their state from recorded sentences ignore it.
    virtual void set_destination(std::optional<model::Destination> /*destination*/) {}

protected:
    Source() = default;
    Source(const Source&) = default;
    Source& operator=(const Source&) = default;
    Source(Source&&) = default;
    Source& operator=(Source&&) = default;
};

}  // namespace nmeasim::core::simulation
