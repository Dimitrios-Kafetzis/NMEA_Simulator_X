#include <nmeasim/core/simulation/sentence_scheduler.hpp>

namespace nmeasim::core::simulation {

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

std::vector<std::string> SentenceScheduler::due(std::chrono::milliseconds now,
                                                const model::VesselState& state) {
    std::vector<std::string> sentences;
    for (const auto& descriptor : registry_->descriptors()) {
        auto& entry = entries_.find(descriptor.id)->second;
        if (!entry.setting.enabled || now < entry.next_due) {
            continue;
        }
        // Schedule relative to now rather than to the missed slot, so a stalled host does not
        // burst-catch-up.
        entry.next_due = now + entry.setting.period;
        auto encoded = nmea0183::encode_within_limit(descriptor, state,
                                                     effective_talker(descriptor), options_);
        sentences.insert(sentences.end(), std::make_move_iterator(encoded.begin()),
                         std::make_move_iterator(encoded.end()));
    }
    return sentences;
}

std::vector<std::string> SentenceScheduler::encode_all(const model::VesselState& state) const {
    std::vector<std::string> sentences;
    for (const auto& descriptor : registry_->descriptors()) {
        if (!entries_.find(descriptor.id)->second.setting.enabled) {
            continue;
        }
        auto encoded = nmea0183::encode_within_limit(descriptor, state,
                                                     effective_talker(descriptor), options_);
        sentences.insert(sentences.end(), std::make_move_iterator(encoded.begin()),
                         std::make_move_iterator(encoded.end()));
    }
    return sentences;
}

void SentenceScheduler::reset() {
    for (auto& [id, entry] : entries_) {
        entry.next_due = std::chrono::milliseconds{0};
    }
}

}  // namespace nmeasim::core::simulation
