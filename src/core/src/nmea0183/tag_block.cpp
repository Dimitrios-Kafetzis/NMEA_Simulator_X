#include <nmeasim/core/nmea0183/checksum.hpp>
#include <nmeasim/core/nmea0183/tag_block.hpp>

#include <cstddef>
#include <format>

namespace nmeasim::core::nmea0183 {

namespace {

constexpr std::size_t kMaxSourceLength{15};

}  // namespace

std::string sanitize_tag_source(std::string_view source) {
    std::string result;
    for (const char c : source) {
        const bool printable = c > ' ' && c <= '~';
        const bool reserved = c == ',' || c == '*' || c == '\\' || c == '!' || c == '$';
        if (printable && !reserved) {
            result += c;
        }
        if (result.size() == kMaxSourceLength) {
            break;
        }
    }
    return result.empty() ? "SIM" : result;
}

std::string format_tag_block(const TagBlockOptions& options,
                             std::chrono::system_clock::time_point time) {
    std::string body = "s:" + sanitize_tag_source(options.source);
    if (options.include_time) {
        const auto since_epoch = time.time_since_epoch();
        const long long value =
            options.milliseconds
                ? std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch).count()
                : std::chrono::duration_cast<std::chrono::seconds>(since_epoch).count();
        body += std::format(",c:{}", value);
    }
    return "\\" + body + "*" + format_checksum(compute_checksum(body)) + "\\";
}

std::string prepend_tag_block(std::string_view sentence, const TagBlockOptions& options,
                              std::chrono::system_clock::time_point time) {
    return format_tag_block(options, time) + std::string{sentence};
}

}  // namespace nmeasim::core::nmea0183
