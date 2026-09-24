// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Implementation of the sentence schedule and of the `next_due_after` cadence rule.

#include <nmeasim/core/simulation/sentence_scheduler.hpp>

#include <chrono>
#include <cstddef>
#include <string>

namespace nmeasim::core::simulation {

std::chrono::milliseconds next_due_after(std::chrono::milliseconds due,
                                         std::chrono::milliseconds now,
                                         std::chrono::milliseconds period) noexcept {
    const auto next = due + period;
    return next > now ? next : now + period;
}

SentenceScheduler::SentenceScheduler(const nmea0183::SentenceRegistry& registry)
    : registry_(&registry) {
    for (const auto& descriptor : registry.descriptors()) {
        entries_.emplace(std::string{descriptor.id},
                         Entry{SentenceSetting{descriptor.enabled_by_default, std::string{},
                                               descriptor.default_period},
                               std::chrono::milliseconds{0}});
    }
}

SentenceSetting SentenceScheduler::setting(std::string_view id) const {
    const auto it = entries_.find(id);
    if (it == entries_.end()) {
        return SentenceSetting{false, std::string{}, std::chrono::milliseconds{0}};
    }
    return it->second.setting;
}

void SentenceScheduler::configure(std::string_view id, SentenceSetting setting) {
    const auto it = entries_.find(id);
    if (it == entries_.end()) {
        return;
    }
    if (setting.period.count() <= 0) {
        setting.period = registry_->find(id)->default_period;
    }
    it->second.setting = std::move(setting);
}

void SentenceScheduler::set_enabled(std::string_view id, bool enabled) {
    const auto it = entries_.find(id);
    if (it != entries_.end()) {
        it->second.setting.enabled = enabled;
    }
}

void SentenceScheduler::set_group_enabled(nmea0183::SentenceGroup group, bool enabled) {
    for (const auto& descriptor : registry_->descriptors()) {
        if (descriptor.group == group) {
            set_enabled(descriptor.id, enabled);
        }
    }
}

void SentenceScheduler::set_period_for_all(std::chrono::milliseconds period) {
    if (period.count() <= 0) {
        return;
    }
    for (auto& [id, entry] : entries_) {
        entry.setting.period = period;
    }
}

std::string_view SentenceScheduler::effective_talker(
    const nmea0183::SentenceDescriptor& descriptor) const {
    const auto it = entries_.find(descriptor.id);
    if (it != entries_.end() && it->second.setting.talker.size() == 2) {
        return it->second.setting.talker;
    }
    return descriptor.default_talker;
}

namespace {

/// Appends encoded sentences to a step's output, tagging each with the id that produced it.
///
/// @param[in,out] sentences The output of the step, extended at the end.
/// @param id The registry id to tag the sentences with.
/// @param encoded The lines one encoder produced for one emission, moved into `sentences`.
void append_encoded(std::vector<EmittedSentence>& sentences, std::string_view id,
                    std::vector<std::string> encoded) {
    for (auto& text : encoded) {
        sentences.push_back({std::string{id}, std::move(text)});
    }
}

}  // namespace

void SentenceScheduler::set_custom_sentences(const std::vector<CustomSentence>& sentences) {
    custom_.clear();
    custom_entries_.clear();
    std::size_t number = 0;
    for (const auto& sentence : sentences) {
        ++number;
        CustomSentence accepted = sentence;
        if (accepted.id.empty()) {
            accepted.id = "CUSTOM-" + std::to_string(number);
        }
        const auto framed = frame_custom_sentence(accepted.body);
        if (!framed || registry_->find(accepted.id) != nullptr) {
            continue;
        }
        if (accepted.period.count() <= 0) {
            accepted.period = std::chrono::milliseconds{1000};
        }
        custom_.push_back(accepted);
        custom_entries_.push_back({accepted, *framed, std::chrono::milliseconds{0}});
    }
}

std::vector<EmittedSentence> SentenceScheduler::due(std::chrono::milliseconds now,
                                                    const model::VesselState& state) {
    std::vector<EmittedSentence> sentences;
    for (const auto& descriptor : registry_->descriptors()) {
        auto& entry = entries_.find(descriptor.id)->second;
        if (!entry.setting.enabled || now < entry.next_due) {
            continue;
        }
        entry.next_due = next_due_after(entry.next_due, now, entry.setting.period);
        append_encoded(sentences, descriptor.id,
                       nmea0183::encode_within_limit(descriptor, state,
                                                     effective_talker(descriptor), options_));
    }
    for (auto& entry : custom_entries_) {
        if (!entry.sentence.enabled || now < entry.next_due) {
            continue;
        }
        entry.next_due = next_due_after(entry.next_due, now, entry.sentence.period);
        sentences.push_back({entry.sentence.id, entry.framed});
    }
    return sentences;
}

std::vector<EmittedSentence> SentenceScheduler::encode_all(const model::VesselState& state) const {
    std::vector<EmittedSentence> sentences;
    for (const auto& descriptor : registry_->descriptors()) {
        if (!entries_.find(descriptor.id)->second.setting.enabled) {
            continue;
        }
        append_encoded(sentences, descriptor.id,
                       nmea0183::encode_within_limit(descriptor, state,
                                                     effective_talker(descriptor), options_));
    }
    for (const auto& entry : custom_entries_) {
        if (entry.sentence.enabled) {
            sentences.push_back({entry.sentence.id, entry.framed});
        }
    }
    return sentences;
}

void SentenceScheduler::reset() {
    for (auto& [id, entry] : entries_) {
        entry.next_due = std::chrono::milliseconds{0};
    }
    for (auto& entry : custom_entries_) {
        entry.next_due = std::chrono::milliseconds{0};
    }
}

}  // namespace nmeasim::core::simulation
