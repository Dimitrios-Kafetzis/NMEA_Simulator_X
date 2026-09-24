// SPDX-License-Identifier: GPL-3.0-only
/// @file
/// Tests of the sentence log reader and writer of `nmeasim/core/log/log_file.hpp`.
///
/// Covers nmeasim::core::log::parse_log() with each timing source (recorder timestamps, Unix
/// time prefixes, TAG block times, sentence time fields and the fixed interval), steps back
/// in the sentence times, its tolerance of junk lines, TAG blocks with a wrong checksum, its
/// rejection of logs without sentences and of a negative interval,
/// nmeasim::core::log::load_log(), and the round trip through
/// nmeasim::core::log::format_log_line(), nmeasim::core::log::format_header_line() and
/// nmeasim::core::log::kHeaderLine.
///
/// Reads the fixtures `tests/fixtures/logs/recorded.log`, `plain.nmea`, `midnight.nmea`,
/// `untimed.nmea`, `unix_prefix.log`, `tagblock.log`, `mixed.log`, `garbage.txt` and
/// `empty.log` in the same directory, and expects `missing.log` not to exist.

#include "core/fixtures.hpp"

#include <nmeasim/core/log/log_file.hpp>
#include <nmeasim/core/time/iso8601.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

using namespace std::chrono_literals;
using nmeasim::core::time::parse_iso8601;
using nmeasim::test::fixture_path;
using nmeasim::test::read_fixture;
namespace logfile = nmeasim::core::log;

namespace {

/// Parses a log fixture that must be accepted.
///
/// Fails the running test case (`REQUIRE`) when nmeasim::core::log::parse_log() rejects the
/// log, and reports its error message.
///
/// @param name Path of the fixture relative to `tests/fixtures/`, such as
///        `logs/recorded.log`; a missing file reads as an empty log.
/// @param interval Spacing of the entries when the log carries no time at all; the 100 ms
///        default is that of nmeasim::core::log::LogParseOptions.
/// @return The parsed log.
logfile::Log parse_fixture(const char* name, std::chrono::milliseconds interval = 100ms) {
    std::string error;
    const auto parsed = logfile::parse_log(read_fixture(name), {interval}, &error);
    INFO(error);
    REQUIRE(parsed.has_value());
    return *parsed;
}

}  // namespace

TEST_CASE("a recorded log keeps its header and absolute timestamps", "[log]") {
    const auto log = parse_fixture("logs/recorded.log");
    CHECK(log.timing == logfile::TimingSource::Timestamps);
    CHECK(log.header.at("format") == "1");
    CHECK(log.header.at("profile") == "Fixture profile");
    CHECK(log.header.at("recorded") == "2026-09-23T10:00:00.000Z");
    CHECK(log.skipped_lines == 0);
    REQUIRE(log.entries.size() == 32);
    CHECK(log.entries[0].offset == 0ms);
    CHECK(log.entries[0].sentence.starts_with("$GPRMC,100000.00"));
    CHECK(log.entries[0].recorded_at == parse_iso8601("2026-09-23T10:00:00Z"));
    // The fixture's recorder timestamps are 20 ms apart within a burst, with bursts every
    // 500 ms.
    CHECK(log.entries[1].offset == 20ms);
    CHECK(log.entries[8].offset == 500ms);
    CHECK(log.entries[31].offset == 1640ms);
    CHECK(log.duration() == 1640ms);
}

TEST_CASE("a plain log is timed from the sentences' own time fields", "[log]") {
    const auto log = parse_fixture("logs/plain.nmea");
    CHECK(log.timing == logfile::TimingSource::SentenceTimes);
    REQUIRE(log.entries.size() == 32);
    CHECK(log.entries[0].offset == 0ms);
    CHECK_FALSE(log.entries[0].recorded_at.has_value());
    // Sentences without a time field share the offset of the last timed one.
    CHECK(log.entries[7].offset == 0ms);
    CHECK(log.entries[8].offset == 500ms);
    CHECK(log.entries[16].offset == 1000ms);
    CHECK(log.duration() == 1500ms);
}

TEST_CASE("sentence times wrap correctly at midnight", "[log]") {
    // RMC times 23:59:58, 23:59:59, 00:00:00 and 00:00:01.50, with an HDT and a DPT between
    // them that share the offset of the RMC before them.
    const auto log = parse_fixture("logs/midnight.nmea");
    CHECK(log.timing == logfile::TimingSource::SentenceTimes);
    REQUIRE(log.entries.size() == 6);
    CHECK(log.entries[1].offset == 0ms);
    CHECK(log.entries[2].offset == 1000ms);
    CHECK(log.entries[3].offset == 2000ms);
    CHECK(log.entries[4].offset == 2000ms);
    CHECK(log.entries[5].offset == 3500ms);
}

TEST_CASE("a step back in the sentence times is made up without doubling time", "[log]") {
    // RMC times 10:00:00, 10:00:10, 10:00:05 and 10:00:15: the step back holds the replay,
    // and 10:00:15 is 15 seconds after the start, not 10 + (15 - 5).
    std::string error;
    const auto log = logfile::parse_log(
        "$GPRMC,100000.00,A,3759.0280,N,02343.6500,E,6.5,45.0,230926,4.6,E,A*0F\n"
        "$GPRMC,100010.00,A,3759.0280,N,02343.6500,E,6.5,45.0,230926,4.6,E,A*0E\n"
        "$GPRMC,100005.00,A,3759.0280,N,02343.6500,E,6.5,45.0,230926,4.6,E,A*0A\n"
        "$GPRMC,100015.00,A,3759.0280,N,02343.6500,E,6.5,45.0,230926,4.6,E,A*0B\n",
        {}, &error);
    REQUIRE(log.has_value());
    CHECK(log->timing == logfile::TimingSource::SentenceTimes);
    REQUIRE(log->entries.size() == 4);
    CHECK(log->entries[1].offset == 10s);
    CHECK(log->entries[2].offset == 10s);
    CHECK(log->entries[3].offset == 15s);

    // 23:59:50, 23:59:45 and 00:00:05: after the step back, midnight is still crossed once.
    const auto midnight = logfile::parse_log(
        "$GPRMC,235950.00,A,3759.0280,N,02343.6500,E,6.5,45.0,230926,4.6,E,A*06\n"
        "$GPRMC,235945.00,A,3759.0280,N,02343.6500,E,6.5,45.0,230926,4.6,E,A*02\n"
        "$GPRMC,000005.00,A,3759.0280,N,02343.6500,E,6.5,45.0,230926,4.6,E,A*0B\n",
        {}, &error);
    REQUIRE(midnight.has_value());
    REQUIRE(midnight->entries.size() == 3);
    CHECK(midnight->entries[1].offset == 0s);
    CHECK(midnight->entries[2].offset == 15s);
}

TEST_CASE("a negative fixed interval is rejected", "[log]") {
    std::string error;
    CHECK_FALSE(logfile::parse_log("$HEHDT,45.0,T\n", {-100ms}, &error).has_value());
    CHECK(error == "The fixed interval must not be negative");
    // Zero puts every entry at offset zero.
    const auto zero = logfile::parse_log("$HEHDT,45.0,T\n$HEHDT,46.0,T\n", {0ms}, &error);
    REQUIRE(zero.has_value());
    CHECK(zero->duration() == 0ms);
}

TEST_CASE("a line whose TAG block checksum is wrong is skipped and counted", "[log]") {
    // Checksums of the TAG blocks computed in Python: 2E for `s:GP0001,c:1790416800`.
    std::string error;
    const auto log = logfile::parse_log(
        "\\s:GP0001,c:1790416800*2E\\$HEHDT,45.0,T\n"
        "\\s:GP0001,c:1790416801*00\\$HEHDT,46.0,T\n"
        "\\s:GP0001,c:1790416801*G1\\$HEHDT,47.0,T\n"
        "\\s:GP0001,c:1790416802\\$HEHDT,48.0,T\n",
        {}, &error);
    REQUIRE(log.has_value());
    // The wrong and the malformed checksum; a block without checksum is accepted.
    CHECK(log->skipped_lines == 2);
    REQUIRE(log->entries.size() == 2);
    CHECK(log->entries[0].sentence == "$HEHDT,45.0,T");
    CHECK(log->entries[1].sentence == "$HEHDT,48.0,T");
    CHECK(log->entries[1].offset == 2s);
}

TEST_CASE("a log without any time is spaced at a fixed interval", "[log]") {
    const auto log = parse_fixture("logs/untimed.nmea", 250ms);
    CHECK(log.timing == logfile::TimingSource::FixedInterval);
    REQUIRE(log.entries.size() == 6);
    CHECK(log.entries[1].offset == 250ms);
    CHECK(log.duration() == 1250ms);
    CHECK(std::string{logfile::to_string(log.timing)} == "fixed interval");
}

TEST_CASE("Unix time prefixes and TAG block times are understood", "[log]") {
    const auto unix = parse_fixture("logs/unix_prefix.log");
    CHECK(unix.timing == logfile::TimingSource::Timestamps);
    REQUIRE(unix.entries.size() == 32);
    // The fixture starts at 1790416800 Unix seconds, which is 2026-09-26T10:00:00Z.
    CHECK(unix.entries[0].recorded_at == parse_iso8601("2026-09-26T10:00:00Z"));
    CHECK(unix.entries[9].offset == 520ms);
    CHECK(unix.duration() == 1640ms);
    // The RMC sentences carry the date of their prefixes, 26 September 2026.
    CHECK(unix.entries[0].sentence.find(",260926,") != std::string::npos);
    CHECK(unix.entries[24].sentence.find(",260926,") != std::string::npos);

    const auto tagged = parse_fixture("logs/tagblock.log");
    CHECK(tagged.timing == logfile::TimingSource::Timestamps);
    REQUIRE(tagged.entries.size() == 8);
    CHECK(tagged.entries[0].sentence.starts_with("$GPRMC"));
    CHECK(tagged.entries[7].offset == 7000ms);  // the c: times step by one second

    std::string error;
    const auto millis = logfile::parse_log(
        "1790416800000 $HEHDT,45.0,T\n1790416800500 $HEHDT,46.0,T\n", {}, &error);
    REQUIRE(millis.has_value());
    CHECK(millis->entries[1].offset == 500ms);
    const auto dated = logfile::parse_log(
        "2026-09-23 10:00:00 $HEHDT,45.0,T\n"
        "2026-09-23 10:00:02,$HEHDT,46.0,T\n",
        {}, &error);
    REQUIRE(dated.has_value());
    CHECK(dated->timing == logfile::TimingSource::Timestamps);
    CHECK(dated->entries[1].offset == 2000ms);
}

TEST_CASE("junk, comments, bad checksums and backwards times are tolerated", "[log]") {
    const auto log = parse_fixture("logs/mixed.log");
    CHECK(log.timing == logfile::TimingSource::Timestamps);
    // The line without a sentence and the RMC with a wrong checksum; the comment and the
    // blank line are not counted.
    CHECK(log.skipped_lines == 2);
    CHECK(log.header.empty());
    REQUIRE(log.entries.size() == 5);
    // The `[10:00:00]` prefix of the first RMC is not a time the reader understands, so the
    // origin is the next line, 10:00:01.
    CHECK(log.entries[0].sentence.starts_with("$GPRMC"));
    CHECK(log.entries[0].offset == 0ms);
    CHECK(log.entries[1].offset == 0ms);
    // An earlier timestamp never moves the replay backwards.
    CHECK(log.entries[2].sentence.starts_with("$SDDPT"));
    CHECK(log.entries[2].offset == 0ms);
    CHECK(log.entries[3].sentence == "$HEHDT,47.0,T");  // accepted without a checksum
    CHECK(log.entries[3].offset == 1000ms);
    CHECK(log.entries[4].offset == 2000ms);
}

TEST_CASE("logs without sentences are rejected", "[log]") {
    std::string error;
    CHECK_FALSE(logfile::parse_log(read_fixture("logs/garbage.txt"), {}, &error).has_value());
    CHECK(error.find("No valid NMEA sentence") != std::string::npos);
    CHECK_FALSE(logfile::parse_log("", {}, &error).has_value());
    CHECK(error.find("empty") != std::string::npos);
    CHECK_FALSE(logfile::parse_log("# only: header\n", {}, nullptr).has_value());
    // The TAG block is never closed by a second backslash, so the only line is skipped.
    CHECK_FALSE(logfile::parse_log("\\s:GP0001,c:1*00 $HEHDT,45.0,T\n", {}, &error).has_value());
}

TEST_CASE("logs are loaded from disk", "[log]") {
    std::string error;
    const auto loaded = logfile::load_log(fixture_path("logs/recorded.log"), {}, &error);
    REQUIRE(loaded.has_value());
    CHECK(loaded->entries.size() == 32);
    CHECK_FALSE(logfile::load_log(fixture_path("logs/missing.log"), {}, &error).has_value());
    CHECK(error.find("Cannot read") != std::string::npos);
    CHECK_FALSE(logfile::load_log(fixture_path("logs/empty.log"), {}, &error).has_value());
    CHECK(error.find("empty.log") != std::string::npos);  // the reason names the file
}

TEST_CASE("log lines are formatted the way the reader expects", "[log]") {
    const auto at = *parse_iso8601("2026-09-23T10:34:56.780Z");
    const auto line = logfile::format_log_line(at, "$HEHDT,45.0,T*1E");
    CHECK(line == "2026-09-23T10:34:56.780Z $HEHDT,45.0,T*1E");
    CHECK(logfile::format_header_line("profile", "Harbour") == "# profile: Harbour");
    CHECK(logfile::kHeaderLine == "# NMEA Simulator X log 1");

    std::string text{logfile::kHeaderLine};
    text += "\n";
    text += logfile::format_header_line("profile", "Harbour");
    text += "\n";
    text += line;
    text += "\n";
    text += logfile::format_log_line(at + 1500ms, "$HEHDT,46.0,T*1D");
    text += "\n";
    std::string error;
    const auto parsed = logfile::parse_log(text, {}, &error);
    REQUIRE(parsed.has_value());
    CHECK(parsed->header.at("format") == "1");
    CHECK(parsed->header.at("profile") == "Harbour");
    REQUIRE(parsed->entries.size() == 2);
    CHECK(parsed->entries[1].offset == 1500ms);
    CHECK(parsed->entries[1].recorded_at == at + 1500ms);
}
