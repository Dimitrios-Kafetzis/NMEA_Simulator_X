// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The unit of output of a simulation step: one sentence together with the id that produced
/// it.

#pragma once

#include <string>

namespace nmeasim::core::simulation {

/// One sentence produced by a simulation step, together with the id that produced it.
///
/// The id lets the outputs filter by sentence even when two registry ids share a formatter,
/// such as `MWV-R` and `MWV-T`.
struct EmittedSentence {
    /// Who produced the sentence.
    ///
    /// For an encoded sentence, the registry id (such as `MWV-R`); every part of a sentence
    /// that is split over several lines, such as GSV, carries the same id. For a custom
    /// sentence, its `CustomSentence::id`. For a replayed sentence, the formatter as decoded
    /// (such as `MWV`), or empty when the recorded line could not be parsed.
    std::string id;
    /// The complete sentence without line terminator.
    ///
    /// An encoded or custom sentence runs from the start delimiter to its checksum; a
    /// replayed sentence is the recorded line exactly as it is in the log.
    std::string text;
};

}  // namespace nmeasim::core::simulation
