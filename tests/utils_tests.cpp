// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline unit tests for the timestamp, formatting and query-string helpers.
// These run without credentials and without network access.

#include <gtest/gtest.h>

#include <alpaca/common.hpp>
#include <alpaca/utils.hpp>

namespace alpaca::tests {

namespace {
constexpr uint64_t ns_per_s = 1'000'000'000ull;
}

// ---------------------------------------------------------------------------
// RFC-3339 parsing
// ---------------------------------------------------------------------------

TEST(TimestampParsing, EpochIsZero) {
    EXPECT_EQ(to_nanoseconds("1970-01-01T00:00:00Z"), 0ull);
}

TEST(TimestampParsing, WholeSeconds) {
    // 2022-01-03T09:00:00Z == 1641200400
    EXPECT_EQ(to_nanoseconds("2022-01-03T09:00:00Z"), 1641200400ull * ns_per_s);
}

TEST(TimestampParsing, MillisecondFraction) {
    EXPECT_EQ(to_nanoseconds("2022-01-03T09:00:00.123Z"),
              1641200400ull * ns_per_s + 123'000'000ull);
}

TEST(TimestampParsing, MicrosecondFraction) {
    EXPECT_EQ(to_nanoseconds("2021-03-16T18:39:00.000001Z"),
              to_nanoseconds("2021-03-16T18:39:00Z") + 1'000ull);
}

TEST(TimestampParsing, NanosecondFraction) {
    // Alpaca's market data stream reports full nanosecond precision.
    EXPECT_EQ(to_nanoseconds("2021-02-06T13:04:56.334320128Z"),
              to_nanoseconds("2021-02-06T13:04:56Z") + 334'320'128ull);
}

TEST(TimestampParsing, FractionLongerThanNanosecondsIsTruncated) {
    // More than 9 fractional digits must not overflow into the seconds field.
    EXPECT_EQ(to_nanoseconds("2021-02-06T13:04:56.3343201289999Z"),
              to_nanoseconds("2021-02-06T13:04:56Z") + 334'320'128ull);
}

TEST(TimestampParsing, NegativeUtcOffset) {
    // 13:30 at UTC-04:00 is 17:30 UTC.
    EXPECT_EQ(to_nanoseconds("2022-04-13T13:30:00-04:00"),
              to_nanoseconds("2022-04-13T17:30:00Z"));
}

TEST(TimestampParsing, PositiveUtcOffset) {
    EXPECT_EQ(to_nanoseconds("2022-04-13T13:30:00+02:00"),
              to_nanoseconds("2022-04-13T11:30:00Z"));
}

TEST(TimestampParsing, OffsetWithoutColon) {
    EXPECT_EQ(to_nanoseconds("2022-04-13T13:30:00-0400"),
              to_nanoseconds("2022-04-13T17:30:00Z"));
}

TEST(TimestampParsing, BareDateIsUtcMidnight) {
    EXPECT_EQ(to_nanoseconds("2022-01-03"), to_nanoseconds("2022-01-03T00:00:00Z"));
}

TEST(TimestampParsing, LeapDay) {
    EXPECT_EQ(to_nanoseconds("2024-02-29T00:00:00Z"),
              to_nanoseconds("2024-02-28T00:00:00Z") + 86400ull * ns_per_s);
}

TEST(TimestampParsing, MissingSecondsField) {
    EXPECT_EQ(to_nanoseconds("2022-01-03T09:00Z"), to_nanoseconds("2022-01-03T09:00:00Z"));
}

TEST(TimestampParsing, EmptyAndMalformedInputsYieldZero) {
    EXPECT_EQ(to_nanoseconds(""), 0ull);
    EXPECT_EQ(to_nanoseconds("not-a-timestamp"), 0ull);
    EXPECT_EQ(to_nanoseconds("2022-01"), 0ull);
    EXPECT_EQ(to_nanoseconds("20220103T090000Z"), 0ull);
}

TEST(TimestampParsing, DerivedUnitsMatchNanoseconds) {
    constexpr std::string_view iso = "2021-02-06T13:04:56.334320128Z";
    EXPECT_EQ(to_microseconds(iso), to_nanoseconds(iso) / 1'000ull);
    EXPECT_EQ(to_milliseconds(iso), to_nanoseconds(iso) / 1'000'000ull);
}

TEST(TimestampParsing, TimeOfDay) {
    EXPECT_EQ(time_of_day_to_nanoseconds("09:30"), (9 * 3600 + 30 * 60) * ns_per_s);
    EXPECT_EQ(time_of_day_to_nanoseconds("16:00"), 16ull * 3600 * ns_per_s);
    EXPECT_EQ(time_of_day_to_nanoseconds("13:00:30"), (13 * 3600 + 30) * ns_per_s);
}

// ---------------------------------------------------------------------------
// RFC-3339 formatting
// ---------------------------------------------------------------------------

TEST(TimestampFormatting, RoundTripsThroughParsing) {
    for (std::string_view iso : {"1970-01-01T00:00:00Z",
                                 "2022-01-03T09:00:00Z",
                                 "2024-02-29T23:59:59Z",
                                 "2021-02-06T13:04:56.334320128Z"}) {
        const uint64_t ns = to_nanoseconds(iso);
        EXPECT_EQ(to_nanoseconds(to_rfc3339(ns)), ns) << iso;
    }
}

TEST(TimestampFormatting, ProducesNanosecondPrecisionUtc) {
    EXPECT_EQ(to_rfc3339(0), "1970-01-01T00:00:00.000000000Z");
    EXPECT_EQ(to_rfc3339(1641200400ull * ns_per_s), "2022-01-03T09:00:00.000000000Z");
}

TEST(TimestampFormatting, DateString) {
    EXPECT_EQ(to_date_string(to_nanoseconds("2022-01-03T09:00:00Z")), "2022-01-03");
    EXPECT_EQ(to_date_string(0), "1970-01-01");
}

// ---------------------------------------------------------------------------
// Number formatting
//
// Alpaca rejects scientific notation in qty/price fields, so this must never emit it.
// ---------------------------------------------------------------------------

TEST(NumberFormatting, TrimsTrailingZeros) {
    EXPECT_EQ(to_string(1.0), "1");
    EXPECT_EQ(to_string(1.50), "1.5");
    EXPECT_EQ(to_string(178.954244), "178.954244");
}

TEST(NumberFormatting, NeverUsesScientificNotation) {
    EXPECT_EQ(to_string(0.00001), "0.00001");
    EXPECT_EQ(to_string(0.000000001), "0.000000001");
    EXPECT_EQ(to_string(1e7), "10000000");
}

TEST(NumberFormatting, HandlesNegativeAndZero) {
    EXPECT_EQ(to_string(0.0), "0");
    EXPECT_EQ(to_string(-2.25), "-2.25");
}

TEST(NumberFormatting, NonFiniteYieldsEmpty) {
    EXPECT_TRUE(to_string(std::nan("")).empty());
    EXPECT_TRUE(to_string(std::numeric_limits<double>::infinity()).empty());
}

TEST(FloatingPointHelpers, FixFloatingError) {
    EXPECT_DOUBLE_EQ(fix_floating_error(0.1 + 0.2), 0.3);
    EXPECT_DOUBLE_EQ(fix_floating_error(-0.3), -0.3);
}

TEST(FloatingPointHelpers, ComputeNumberDecimals) {
    EXPECT_EQ(compute_number_decimals(1.0), 0u);
    EXPECT_EQ(compute_number_decimals(0.01), 2u);
    EXPECT_EQ(compute_number_decimals(0.0001), 4u);
}

// ---------------------------------------------------------------------------
// URL encoding and query building
// ---------------------------------------------------------------------------

TEST(UrlEncode, LeavesUnreservedCharactersAlone) {
    EXPECT_EQ(url_encode("AAPL"), "AAPL");
    EXPECT_EQ(url_encode("a-b_c.d~e"), "a-b_c.d~e");
}

TEST(UrlEncode, EncodesReservedCharacters) {
    EXPECT_EQ(url_encode("a b"), "a%20b");
    EXPECT_EQ(url_encode("2022-01-03T09:00:00Z"), "2022-01-03T09%3A00%3A00Z");
    EXPECT_EQ(url_encode("a&b=c"), "a%26b%3Dc");
}

TEST(QueryBuilder, EmptyBuilderProducesEmptyString) {
    query_builder q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.str(), "");
}

TEST(QueryBuilder, SingleAndMultipleParameters) {
    query_builder q;
    q.add("limit", 50);
    EXPECT_EQ(q.str(), "?limit=50");

    q.add("status", "open");
    EXPECT_EQ(q.str(), "?limit=50&status=open");
}

TEST(QueryBuilder, UnsetOptionalsAreSkipped) {
    query_builder q;
    q.add("limit", std::optional<uint32_t>{});
    q.add("cursor", std::optional<std::string>{});
    EXPECT_TRUE(q.empty());

    q.add("limit", std::optional<uint32_t>{10});
    EXPECT_EQ(q.str(), "?limit=10");
}

TEST(QueryBuilder, EmptyStringsAreSkipped) {
    // An empty symbol filter must not become "symbols=", which Alpaca rejects.
    query_builder q;
    q.add("symbols", "");
    EXPECT_TRUE(q.empty());
}

TEST(QueryBuilder, VectorsFlattenToCsv) {
    query_builder q;
    q.add("symbols", std::vector<std::string>{"AAPL", "MSFT", "TSLA"});
    EXPECT_EQ(q.str(), "?symbols=AAPL%2CMSFT%2CTSLA");
}

TEST(QueryBuilder, EmptyVectorIsSkipped) {
    query_builder q;
    q.add("symbols", std::vector<std::string>{});
    EXPECT_TRUE(q.empty());
}

TEST(QueryBuilder, BooleansUseWireSpelling) {
    query_builder q;
    q.add("nested", true);
    q.add("cancel_orders", false);
    EXPECT_EQ(q.str(), "?nested=true&cancel_orders=false");
}

TEST(QueryBuilder, DoublesAvoidScientificNotation) {
    query_builder q;
    q.add("strike_price_gte", 0.00001);
    EXPECT_EQ(q.str(), "?strike_price_gte=0.00001");
}

TEST(QueryBuilder, EnumsUseWireSpelling) {
    query_builder q;
    q.add("status", order_status_filter::open);
    q.add("direction", sort_direction::desc);
    EXPECT_EQ(q.str(), "?status=open&direction=desc");
}

TEST(QueryBuilder, SetReplacesInPlaceWithoutReordering) {
    // Pagination relies on swapping page_token without appending a duplicate key.
    query_builder q;
    q.add("symbols", "AAPL");
    q.add("page_token", "first");
    q.add("limit", 100);

    q.set("page_token", "second");
    EXPECT_EQ(q.str(), "?symbols=AAPL&page_token=second&limit=100");
}

TEST(QueryBuilder, SetAddsWhenKeyIsAbsent) {
    query_builder q;
    q.add("symbols", "AAPL");
    q.set("page_token", "abc");
    EXPECT_EQ(q.str(), "?symbols=AAPL&page_token=abc");
}

TEST(QueryBuilder, Remove) {
    query_builder q;
    q.add("a", "1");
    q.add("b", "2");
    q.remove("a");
    EXPECT_EQ(q.str(), "?b=2");
}

TEST(QueryBuilder, TimestampsAreFormattedAndZeroIsSkipped) {
    query_builder q;
    q.add_timestamp("start", 0);
    EXPECT_TRUE(q.empty());

    q.add_timestamp("start", to_nanoseconds("2022-01-03T09:00:00Z"));
    EXPECT_EQ(q.str(), "?start=2022-01-03T09%3A00%3A00.000000000Z");
}

// ---------------------------------------------------------------------------
// JSON field extraction
// ---------------------------------------------------------------------------

TEST(JsonExtraction, DoubleAcceptsStringAndNumber) {
    // Trading API sends numbers as strings; Market Data API sends them as numbers.
    const json j = {{"as_string", "1.5"}, {"as_number", 1.5}, {"as_null", nullptr}, {"as_empty", ""}};
    EXPECT_DOUBLE_EQ(double_from_json(j, "as_string"), 1.5);
    EXPECT_DOUBLE_EQ(double_from_json(j, "as_number"), 1.5);
    EXPECT_DOUBLE_EQ(double_from_json(j, "as_null"), 0.0);
    EXPECT_DOUBLE_EQ(double_from_json(j, "as_empty"), 0.0);
}

TEST(JsonExtraction, IntAcceptsStringAndNumber) {
    const json j = {{"as_string", "42"}, {"as_number", 42}, {"as_null", nullptr}};
    EXPECT_EQ(int_from_json(j, "as_string"), 42);
    EXPECT_EQ(int_from_json(j, "as_number"), 42);
    EXPECT_EQ(int_from_json(j, "as_null"), 0);
}

TEST(JsonExtraction, BoolAcceptsStringAndBoolean) {
    const json j = {{"as_bool", true}, {"as_string", "true"}, {"as_false_string", "false"},
                    {"as_null", nullptr}};
    EXPECT_TRUE(bool_from_json(j, "as_bool"));
    EXPECT_TRUE(bool_from_json(j, "as_string"));
    EXPECT_FALSE(bool_from_json(j, "as_false_string"));
    EXPECT_FALSE(bool_from_json(j, "as_null"));
}

// ---------------------------------------------------------------------------
// Enum round-tripping
// ---------------------------------------------------------------------------

TEST(Enums, RoundTripThroughWireStrings) {
    EXPECT_EQ(to_order_side("buy"), order_side::buy);
    EXPECT_EQ(to_string(order_side::sell), "sell");
    EXPECT_EQ(to_order_status("partially_filled"), order_status::partially_filled);
    EXPECT_EQ(to_string(order_status::new_), "new");
    EXPECT_EQ(to_position_side("long"), position_side::long_);
    EXPECT_EQ(to_string(position_side::short_), "short");
    EXPECT_EQ(to_activity_type("FILL"), activity_type::fill);
    EXPECT_EQ(to_string(activity_type::int_), "INT");
    EXPECT_EQ(to_crypto_location("us-1"), crypto_location::us_1);
}

TEST(Enums, UnrecognisedValuesBecomeUnknownRatherThanThrowing) {
    // Alpaca adds enumerators over time; an SDK that throws on a new one is worse than
    // one that reports `unknown`.
    EXPECT_EQ(to_order_status("some_future_status"), order_status::unknown);
    EXPECT_EQ(to_order_side(""), order_side::unknown);
    EXPECT_EQ(to_string(order_status::unknown), "");
}

}   // namespace alpaca::tests
