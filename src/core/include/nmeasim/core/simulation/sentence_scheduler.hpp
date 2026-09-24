// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// The sentence schedule: which NMEA 0183 sentences are sent, with which talker and how
/// often, and which are due at a given simulated time.
///
/// `SentenceScheduler::due` is called by `Simulation::step` with the simulated elapsed time;
/// `next_due_after` is the rule that keeps each sentence on its cadence.

#pragma once

#include <nmeasim/core/model/vessel_state.hpp>
#include <nmeasim/core/nmea0183/registry.hpp>
#include <nmeasim/core/simulation/custom_sentence.hpp>
#include <nmeasim/core/simulation/emitted_sentence.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace nmeasim::core::simulation {

/// The settings of one registry sentence that an operator can change.
struct SentenceSetting {
    /// Whether the sentence is emitted at all.
    bool enabled{true};
    /// Talker identifier that replaces the registry default, such as `GN`.
    ///
    /// Empty means the registry default. Only a value that `is_valid_talker` accepts, two
    /// upper-case letters, is used; the scheduler keeps any other value as given but sends
    /// the sentence with the registry default.
    std::string talker;
    /// Interval between emissions in simulated time; `SentenceScheduler::configure` replaces
    /// zero or a negative value by the registry default.
    std::chrono::milliseconds period{1000};
};

/// Returns whether a text is acceptable as the talker of a `SentenceSetting`.
///
/// @param talker The talker as typed.
/// @return True when `talker` is empty, which selects the registry default, or consists of
///   exactly two upper-case ASCII letters `A` to `Z`; false otherwise, including for
///   lower-case letters, digits and the user-configured `U0` to `U9`.
/// @see NMEA 0183 (IEC 61162-1), talker identifier.
[[nodiscard]] bool is_valid_talker(std::string_view talker) noexcept;

/// Returns the next due time of a periodic message that was due at `due` and is sent at
/// `now`.
///
/// The next due time is one period after the slot, `due + period`, so that the cadence
/// follows the simulated clock even though each step that sends the message arrives a little
/// after its slot: a 1000 ms sentence sent by 99 ms steps still goes out once per second
/// without drifting. When `due + period` is not later than `now`, the message has fallen a
/// whole period or more behind (the host stalled), and the next due time is `now + period`
/// instead, so that the message is sent once rather than in a burst of catch-up copies.
///
/// @param due The time the message was due, in simulated time since the start.
/// @param now The time it is being sent, in simulated time since the start; at or after
///   `due`.
/// @param period The message's interval; positive.
/// @return `due + period` when that is later than `now`, otherwise `now + period`.
[[nodiscard]] std::chrono::milliseconds next_due_after(std::chrono::milliseconds due,
                                                       std::chrono::milliseconds now,
                                                       std::chrono::milliseconds period) noexcept;

/// The per-sentence enable flags, talkers and periods, plus the operator's custom sentences,
/// with the time each one is next due.
///
/// There is one entry for every descriptor of the registry, starting at the registry
/// defaults; ids not in the registry are ignored by every setter. The schedule runs on the
/// simulated time passed to `due`: everything enabled is due at the first call, and each
/// sentence is then due again as `next_due_after` decides. Changing a setting does not move
/// the time a sentence is next due.
///
/// The scheduler keeps a pointer to its registry, which must outlive it; the default,
/// `nmea0183::SentenceRegistry::standard()`, lives for the whole program. Copies share the
/// registry.
class SentenceScheduler {
public:
    /// Builds a schedule with every registry sentence at its default enable flag, talker and
    /// period, and no custom sentences.
    ///
    /// @param registry The sentence catalogue to schedule; it must outlive the scheduler.
    explicit SentenceScheduler(
        const nmea0183::SentenceRegistry& registry = nmea0183::SentenceRegistry::standard());

    /// Returns the registry the schedule was built from.
    ///
    /// @return The registry given to the constructor.
    [[nodiscard]] const nmea0183::SentenceRegistry& registry() const noexcept { return *registry_; }

    /// Returns the setting of a sentence.
    ///
    /// @param id Registry id, such as `RMC` or `MWV-T`.
    /// @return The current setting, or a disabled setting with an empty talker and a zero
    ///   period when `id` is not in the registry.
    [[nodiscard]] SentenceSetting setting(std::string_view id) const;
    /// Replaces the setting of a sentence.
    ///
    /// @param id Registry id; an unknown id is ignored.
    /// @param setting The new setting. A zero or negative period is replaced by the
    ///   registry default.
    void configure(std::string_view id, SentenceSetting setting);
    /// Enables or disables one sentence.
    ///
    /// @param id Registry id; an unknown id is ignored.
    /// @param enabled Whether the sentence is emitted.
    void set_enabled(std::string_view id, bool enabled);
    /// Enables or disables every registry sentence of a group.
    ///
    /// @param group The functional group, such as the wind sentences.
    /// @param enabled Whether the sentences of the group are emitted.
    void set_group_enabled(nmea0183::SentenceGroup group, bool enabled);
    /// Sets every registry sentence to the same period; custom sentences keep their own.
    ///
    /// @param period The new interval; zero or a negative value is ignored and changes
    ///   nothing.
    void set_period_for_all(std::chrono::milliseconds period);

    /// Returns the talker a sentence is sent with after overrides.
    ///
    /// @param descriptor The registry descriptor of the sentence.
    /// @return The talker of the sentence's setting when it is not empty and
    ///   `is_valid_talker` accepts it, otherwise the registry default. The view refers either to a
    ///   string held by this scheduler, valid until that sentence's setting changes or the
    ///   scheduler is destroyed, or to the registry's static text.
    [[nodiscard]] std::string_view effective_talker(
        const nmea0183::SentenceDescriptor& descriptor) const;

    /// Returns the formatting options passed to every encoder.
    ///
    /// @return A copy of the options.
    [[nodiscard]] nmea0183::EncoderOptions encoder_options() const noexcept { return options_; }
    /// Replaces the formatting options passed to every encoder.
    ///
    /// @param options The new options, for example the number of position decimals.
    void set_encoder_options(nmea0183::EncoderOptions options) noexcept { options_ = options; }

    /// Replaces the operator's custom sentences.
    ///
    /// Each sentence is taken over with these changes: an empty id becomes `CUSTOM-n`, where
    /// `n` is its 1-based position in `sentences` (`effective_custom_id`); a zero or
    /// negative period becomes one second. A sentence whose body `frame_custom_sentence`
    /// refuses, or whose id (after filling in) is a registry id or the id of a sentence
    /// accepted before it, is dropped. Disabled sentences are kept. The accepted
    /// sentences are due at the next call to `due`; they are emitted after the registry
    /// ones, in list order.
    ///
    /// @param sentences The custom sentences, in the order they are to be emitted; an empty
    ///   list removes them all.
    void set_custom_sentences(const std::vector<CustomSentence>& sentences);
    /// Returns the custom sentences accepted by the last `set_custom_sentences`.
    ///
    /// @return The accepted sentences with ids and periods filled in, in list order. The
    ///   reference is valid until the next `set_custom_sentences` or the scheduler's
    ///   destruction.
    [[nodiscard]] const std::vector<CustomSentence>& custom_sentences() const noexcept {
        return custom_;
    }

    /// Encodes every enabled sentence that is due at `now` and schedules its next emission.
    ///
    /// A sentence is due when `now` is at or after its next due time; that time is then
    /// advanced with `next_due_after`. Each registry sentence is encoded from `state` with
    /// its effective talker and the encoder options, lowering the position precision when a
    /// sentence would otherwise exceed the length limit.
    ///
    /// @param now Simulated time since the start or the last `reset`. Successive calls are
    ///   expected with times that never decrease.
    /// @param state The vessel state to encode.
    /// @return The due sentences without line terminators: registry sentences in registry
    ///   order (a sentence that needs several lines, such as GSV, contributes all of them),
    ///   then custom sentences in list order. Empty when nothing is due.
    [[nodiscard]] std::vector<EmittedSentence> due(std::chrono::milliseconds now,
                                                   const model::VesselState& state);

    /// Encodes every enabled sentence regardless of schedule, for example for a preview.
    ///
    /// The schedule is not changed.
    ///
    /// @param state The vessel state to encode.
    /// @return The same sentences, in the same order, as `due` returns when everything is
    ///   due.
    [[nodiscard]] std::vector<EmittedSentence> encode_all(const model::VesselState& state) const;

    /// Forgets the emission history, so that every enabled sentence, custom ones included, is
    /// due at the next call to `due`.
    ///
    /// Settings and custom sentences are kept.
    void reset();

private:
    /// Schedule of one registry sentence.
    struct Entry {
        /// The operator's setting for the sentence.
        SentenceSetting setting;
        /// Simulated time at which the sentence is next due; zero makes it due at once.
        std::chrono::milliseconds next_due{0};
    };

    /// Schedule of one accepted custom sentence.
    struct CustomEntry {
        /// The sentence as accepted, with id and period filled in.
        CustomSentence sentence;
        /// The body framed once by `frame_custom_sentence`, sent unchanged every time.
        std::string framed;
        /// Simulated time at which the sentence is next due; zero makes it due at once.
        std::chrono::milliseconds next_due{0};
    };

    /// The catalogue scheduled; not owned, never null.
    const nmea0183::SentenceRegistry* registry_;
    /// One entry per registry id, keyed by id; the transparent comparator allows lookup by
    /// `std::string_view`.
    std::map<std::string, Entry, std::less<>> entries_;
    /// Formatting options passed to every encoder.
    nmea0183::EncoderOptions options_{};
    /// The accepted custom sentences, as returned by `custom_sentences`.
    std::vector<CustomSentence> custom_;
    /// The schedule of each accepted custom sentence, in the same order as `custom_`.
    std::vector<CustomEntry> custom_entries_;
};

}  // namespace nmeasim::core::simulation
