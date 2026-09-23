#pragma once

#include <nmeasim/core/model/vessel_state.hpp>

#include <chrono>
#include <optional>

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

protected:
    Source() = default;
    Source(const Source&) = default;
    Source& operator=(const Source&) = default;
    Source(Source&&) = default;
    Source& operator=(Source&&) = default;
};

}  // namespace nmeasim::core::simulation
