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

/// Decides which sentences are due at a given simulation time and encodes them.
namespace nmeasim::core::simulation {

/// Per-sentence settings an operator can change.
struct SentenceSetting {
    /// Whether the sentence is emitted at all.
    bool enabled{true};
    /// Two-character talker; empty means the registry default.
    std::string talker;
    /// Interval between emissions; `configure` replaces zero or negative with the registry
    /// default.
    std::chrono::milliseconds period{1000};
};

/// Next due time of a periodic message that was due at `due` and is sent at `now` (`now` is
/// at or after `due`): one `period` after `due`, so that the cadence follows the simulated
/// clock even when steps arrive slightly late, or one `period` after `now` when the message
/// is more than a period behind, so that a stalled host does not send a burst to catch up.
[[nodiscard]] std::chrono::milliseconds next_due_after(std::chrono::milliseconds due,
                                                       std::chrono::milliseconds now,
                                                       std::chrono::milliseconds period) noexcept;

/// Per-sentence enable flags, talkers and periods, plus the operator's custom sentences, with
/// the time each one is next due.
class SentenceScheduler {
public:
    /// Builds a schedule with every sentence at its registry defaults.
    explicit SentenceScheduler(
        const nmea0183::SentenceRegistry& registry = nmea0183::SentenceRegistry::standard());

    /// The registry the schedule was built from.
    [[nodiscard]] const nmea0183::SentenceRegistry& registry() const noexcept { return *registry_; }

    /// Returns the setting for a sentence id. Unknown ids yield a disabled default.
    [[nodiscard]] SentenceSetting setting(std::string_view id) const;
    /// Replaces the setting for a sentence id. Unknown ids are ignored.
    void configure(std::string_view id, SentenceSetting setting);
    /// Enables or disables one sentence. Unknown ids are ignored.
    void set_enabled(std::string_view id, bool enabled);
    /// Enables or disables every registry sentence of a group.
    void set_group_enabled(nmea0183::SentenceGroup group, bool enabled);
    /// Sets every sentence to the same period. Zero or negative periods are ignored.
    void set_period_for_all(std::chrono::milliseconds period);

    /// The talker that will be used for a sentence after overrides.
    [[nodiscard]] std::string_view effective_talker(
        const nmea0183::SentenceDescriptor& descriptor) const;

    /// Options passed to every encoder.
    [[nodiscard]] nmea0183::EncoderOptions encoder_options() const noexcept { return options_; }
    /// Replaces the options passed to every encoder.
    void set_encoder_options(nmea0183::EncoderOptions options) noexcept { options_ = options; }

    /// Replaces the operator's custom sentences. Bodies that cannot be framed and ids that
    /// clash with a registry id are dropped; empty ids become `CUSTOM-n`, numbered from 1 in
    /// list order. Custom sentences are emitted after the registry ones.
    void set_custom_sentences(const std::vector<CustomSentence>& sentences);
    /// The custom sentences accepted by the last `set_custom_sentences`, with ids filled in.
    [[nodiscard]] const std::vector<CustomSentence>& custom_sentences() const noexcept {
        return custom_;
    }

    /// Encodes every enabled sentence that is due at `now` (time since start) and schedules
    /// its next emission. Sentences are returned without line terminators, in registry order.
    [[nodiscard]] std::vector<EmittedSentence> due(std::chrono::milliseconds now,
                                                   const model::VesselState& state);

    /// Encodes every enabled sentence regardless of schedule, e.g. for a preview.
    [[nodiscard]] std::vector<EmittedSentence> encode_all(const model::VesselState& state) const;

    /// Forgets emission history so that everything is due at the next call.
    void reset();

private:
    struct Entry {
        SentenceSetting setting;
        std::chrono::milliseconds next_due{0};
    };

    struct CustomEntry {
        CustomSentence sentence;
        std::string framed;
        std::chrono::milliseconds next_due{0};
    };

    const nmea0183::SentenceRegistry* registry_;
    std::map<std::string, Entry, std::less<>> entries_;
    nmea0183::EncoderOptions options_{};
    std::vector<CustomSentence> custom_;
    std::vector<CustomEntry> custom_entries_;
};

}  // namespace nmeasim::core::simulation
