#pragma once

#include <string>

namespace nmeasim::core::simulation {

/// One encoded sentence together with the registry id that produced it, so that outputs can
/// filter by id even when two ids share a formatter (MWV-R and MWV-T). Replayed sentences
/// carry the formatter as id.
struct EmittedSentence {
    std::string id;
    std::string text;
};

}  // namespace nmeasim::core::simulation
