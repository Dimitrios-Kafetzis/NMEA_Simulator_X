#include <nmeasim/core/time/iso8601.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using namespace std::chrono;
namespace iso = nmeasim::core::time;

namespace {

system_clock::time_point at(int y, unsigned m, unsigned d, int hh, int mm, int ss, int ms = 0) {
    return sys_days{year{y} / month{m} / day{d}} + hours{hh} + minutes{mm} + seconds{ss} +
           milliseconds{ms};
}

}  // namespace

TEST_CASE("ISO 8601 date-times parse in every common variant", "[time]") {
    CHECK(iso::parse_iso8601("2026-09-23T10:00:00Z") == at(2026, 9, 23, 10, 0, 0));
    CHECK(iso::parse_iso8601("2026-09-23T10:00:00.250Z") == at(2026, 9, 23, 10, 0, 0, 250));
    CHECK(iso::parse_iso8601("2026-09-23T10:00:00.7Z") == at(2026, 9, 23, 10, 0, 0, 700));
    CHECK(iso::parse_iso8601("2026-09-23T10:00:00.123456Z") == at(2026, 9, 23, 10, 0, 0, 123));
    CHECK(iso::parse_iso8601("2026-09-23T10:00:00") == at(2026, 9, 23, 10, 0, 0));
    CHECK(iso::parse_iso8601("2026-09-23 10:00") == at(2026, 9, 23, 10, 0, 0));
    CHECK(iso::parse_iso8601("2026-09-23") == at(2026, 9, 23, 0, 0, 0));
    CHECK(iso::parse_iso8601("  2026-09-23T10:00:00Z\n") == at(2026, 9, 23, 10, 0, 0));
    // Offsets are converted to UTC.
    CHECK(iso::parse_iso8601("2026-09-23T12:00:00+02:00") == at(2026, 9, 23, 10, 0, 0));
    CHECK(iso::parse_iso8601("2026-09-23T12:00:00+0200") == at(2026, 9, 23, 10, 0, 0));
    CHECK(iso::parse_iso8601("2026-09-23T12:00:00+02") == at(2026, 9, 23, 10, 0, 0));
    CHECK(iso::parse_iso8601("2026-09-22T22:30:00-05:00") == at(2026, 9, 23, 3, 30, 0));
    CHECK(iso::parse_iso8601("2024-02-29T00:00:00Z") == at(2024, 2, 29, 0, 0, 0));
}

TEST_CASE("invalid ISO 8601 text is rejected", "[time]") {
    for (const char* text : {"", "yesterday", "2026-13-01T00:00:00Z", "2026-02-30T00:00:00Z",
                             "2026-09-23T25:00:00Z", "2026-09-23T10:61:00Z", "2026-09-23X10:00:00Z",
                             "2026-09-23T10:00:00.Z", "2026-09-23T10:00:00+25:00",
                             "2026-09-23T10:00:00Zjunk", "26-09-23T10:00:00Z", "2026-9-23"}) {
        INFO(text);
        CHECK_FALSE(iso::parse_iso8601(text).has_value());
    }
}

TEST_CASE("time points format as UTC with milliseconds and round-trip", "[time]") {
    const auto point = at(2026, 9, 23, 10, 34, 56, 780);
    CHECK(iso::format_iso8601(point) == "2026-09-23T10:34:56.780Z");
    CHECK(iso::parse_iso8601(iso::format_iso8601(point)) == point);
    CHECK(iso::format_iso8601(at(1999, 1, 2, 3, 4, 5)) == "1999-01-02T03:04:05.000Z");
}
