#pragma once

#include <string>

namespace nmeasim::core::simulation {

/// One encoded sentence together with the registry id that produced it, so that outputs can
/// filter by id even when two ids share a formatter (MWV-R and MWV-T). Replayed sentences
/// carry the formatter as id.
struct EmittedSentence {
    /// Registry id (such as `MWV-R`), custom sentence id, or the formatter of a replayed one.
    std::string id;
    /// The complete sentence without line terminator.
    std::string text;
};

}  // namespace nmeasim::core::simulation
