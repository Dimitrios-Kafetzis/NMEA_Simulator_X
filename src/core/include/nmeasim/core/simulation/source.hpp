#pragma once

#include <nmeasim/core/model/vessel_state.hpp>

#include <chrono>

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

protected:
    Source() = default;
    Source(const Source&) = default;
    Source& operator=(const Source&) = default;
    Source(Source&&) = default;
    Source& operator=(Source&&) = default;
};

}  // namespace nmeasim::core::simulation
