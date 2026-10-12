// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline tests for the MessagePack option stream.
//
// Nothing here opens a socket. Frames are handed straight to the stream's frame handler,
// which is the same entry point the websocket callback uses, so the whole decode and
// dispatch path runs exactly as it does live. Test frames are packed with nlohmann's own
// MessagePack writer — the library under test decodes with msgpack-cxx, so the two ends
// are independent implementations — one frame is spelled out byte by byte, and the
// control frames are also replayed exactly as Alpaca's servers sent them, so the suite
// does not rest on either library's reading of the format.

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include <alpaca/streaming/option_data_stream.hpp>

namespace alpaca::tests {

namespace {

credentials dummy() { return credentials("PKTEST", "secret"); }

/// Opens up the protected framing hooks, which are the seam between the socket and the
/// stream. Deriving is what a real transport test would do too; nothing is bypassed.
class testable_stream : public option_data_stream {
public:
    using option_data_stream::option_data_stream;
    using option_data_stream::auth_frame;
    using option_data_stream::encode;
    using option_data_stream::handle_frame;

    void feed(const std::string &frame) { handle_frame(frame.data(), frame.size()); }
};

std::string pack(const json &frame) {
    const auto bytes = json::to_msgpack(frame);
    return {bytes.begin(), bytes.end()};
}

json unpack(const std::string &frame) {
    return json::from_msgpack(frame.begin(), frame.end());
}

std::string from_hex(std::string_view hex) {
    const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    std::string out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<char>((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
    }
    return out;
}

std::vector<std::uint8_t> big_endian(std::uint64_t value, int bytes) {
    std::vector<std::uint8_t> out;
    for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
    return out;
}

/// A MessagePack extension value. nlohmann writes a binary with a subtype as `ext`.
json ext(std::vector<std::uint8_t> payload, std::uint8_t type) {
    return json::binary(std::move(payload), type);
}

constexpr std::uint8_t timestamp_type = 0xFF;   // extension type -1

/// timestamp 64: 30 bits of nanoseconds above 34 bits of seconds.
json timestamp64(std::uint64_t nanoseconds) {
    const std::uint64_t seconds = nanoseconds / 1'000'000'000ull;
    const std::uint64_t nanos = nanoseconds % 1'000'000'000ull;
    return ext(big_endian((nanos << 34) | seconds, 8), timestamp_type);
}

// 2024-03-11T13:35:35.13312256Z, the trade in Alpaca's own documentation.
constexpr std::uint64_t doc_trade_ns = 1'710'164'135'133'122'560ull;
// 2024-03-12T11:59:38.897261568Z, the documented quote.
constexpr std::uint64_t doc_quote_ns = 1'710'244'778'897'261'568ull;

json doc_trade() {
    return {{"T", "t"}, {"S", "AAPL240315C00172500"}, {"t", timestamp64(doc_trade_ns)},
            {"p", 2.84}, {"s", 1}, {"x", "N"}, {"c", "S"}};
}

json doc_quote() {
    return {{"T", "q"}, {"S", "SPXW240327P04925000"}, {"t", timestamp64(doc_quote_ns)},
            {"bx", "C"}, {"bp", 9.46}, {"bs", 53}, {"ax", "C"}, {"ap", 9.66}, {"as", 38},
            {"c", "A"}};
}

/// What a trade handler saw, copied out because the stream reuses the object it passes.
struct seen_trade {
    std::string symbol;
    trade value;
};

struct seen_quote {
    std::string symbol;
    quote value;
};

}   // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(OptionStreamUrls, FeedIsAPathSegment) {
    EXPECT_EQ(option_data_stream(dummy(), option_feed::indicative).url(),
              "wss://stream.data.alpaca.markets/v1beta1/indicative");
    EXPECT_EQ(option_data_stream(dummy(), option_feed::opra).url(),
              "wss://stream.data.alpaca.markets/v1beta1/opra");
}

TEST(OptionStreamUrls, DefaultsToTheFeedThatNeedsNoSubscription) {
    const option_data_stream stream(dummy());

    EXPECT_EQ(stream.feed(), option_feed::indicative);
    EXPECT_EQ(stream.url(), "wss://stream.data.alpaca.markets/v1beta1/indicative");
}

TEST(OptionStreamUrls, SandboxUsesTheSandboxHost) {
    EXPECT_EQ(option_data_stream(dummy(), option_feed::opra, environment::sandbox).url(),
              "wss://stream.data.sandbox.alpaca.markets/v1beta1/opra");
}

TEST(OptionStreamUrls, FramesAreSentAsBinary) {
    EXPECT_EQ(option_data_stream(dummy()).outbound_frame_type(), detail::frame_type::binary);
}

// ---------------------------------------------------------------------------
// Subscription bookkeeping
// ---------------------------------------------------------------------------

TEST(OptionStreamSubscriptions, TradesAndQuotesAreRecordedUnderTheirWireNames) {
    option_data_stream stream(dummy());
    stream.subscribe_trades({"AAPL240315C00172500", "AAPL240315P00172500"});
    stream.subscribe_quotes({"AAPL240315C00172500"});

    const auto channels = stream.pending_channels();
    ASSERT_TRUE(channels.contains("trades"));
    EXPECT_EQ(channels.at("trades").size(), 2u);
    ASSERT_TRUE(channels.contains("quotes"));
    EXPECT_TRUE(channels.at("quotes").contains("AAPL240315C00172500"));
}

TEST(OptionStreamSubscriptions, UnsubscribeRemovesOnlyTheNamedContracts) {
    option_data_stream stream(dummy());
    stream.subscribe_quotes({"AAPL240315C00172500", "AAPL240315P00172500"});
    stream.unsubscribe_quotes({"AAPL240315P00172500"});

    const auto quotes = stream.pending_channels().at("quotes");
    EXPECT_EQ(quotes.size(), 1u);
    EXPECT_TRUE(quotes.contains("AAPL240315C00172500"));
}

// ---------------------------------------------------------------------------
// Outbound frames
// ---------------------------------------------------------------------------

TEST(OptionStreamEncoding, ControlFramesAreMsgpack) {
    const testable_stream stream(dummy());
    const json frame = {{"action", "subscribe"}, {"quotes", {"AAPL240315C00172500"}}};

    const std::string encoded = stream.encode(frame);

    // A MessagePack map, not the '{' of JSON text.
    ASSERT_FALSE(encoded.empty());
    EXPECT_EQ(static_cast<unsigned char>(encoded.front()) & 0xF0, 0x80);
    EXPECT_EQ(unpack(encoded), frame);
}

TEST(OptionStreamEncoding, AuthFrameCarriesTheSameFieldsAsTheJsonStreams) {
    const testable_stream stream(dummy());

    const json auth = unpack(stream.auth_frame());

    EXPECT_EQ(auth, json::parse(dummy().stream_auth_message()));
    EXPECT_EQ(auth["action"], "auth");
    EXPECT_EQ(auth["key"], "PKTEST");
    EXPECT_EQ(auth["secret"], "secret");
}

TEST(OptionStreamEncoding, OAuthUsesTheDocumentedKeyForm) {
    const testable_stream stream(credentials::from_oauth_token("token-123"));

    const json auth = unpack(stream.auth_frame());

    EXPECT_EQ(auth["key"], "oauth");
    EXPECT_EQ(auth["secret"], "token-123");
}

// ---------------------------------------------------------------------------
// Trades and quotes
// ---------------------------------------------------------------------------

TEST(OptionStreamDecoding, TradeArrivesAsTheRestTradeType) {
    testable_stream stream(dummy());
    std::vector<seen_trade> trades;
    stream.on_trade([&](const std::string &symbol, const trade &t) {
        trades.push_back({symbol, t});
    });

    stream.feed(pack(json::array({doc_trade()})));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].symbol, "AAPL240315C00172500");
    EXPECT_EQ(trades[0].value.timestamp, doc_trade_ns);
    EXPECT_EQ(trades[0].value.timestamp, to_nanoseconds("2024-03-11T13:35:35.13312256Z"));
    EXPECT_DOUBLE_EQ(trades[0].value.price, 2.84);
    EXPECT_DOUBLE_EQ(trades[0].value.size, 1.);
    EXPECT_EQ(trades[0].value.exchange, "N");
    EXPECT_EQ(trades[0].value.conditions, std::vector<std::string>{"S"});
}

TEST(OptionStreamDecoding, QuoteArrivesAsTheRestQuoteType) {
    testable_stream stream(dummy());
    std::vector<seen_quote> quotes;
    stream.on_quote([&](const std::string &symbol, const quote &q) {
        quotes.push_back({symbol, q});
    });

    stream.feed(pack(json::array({doc_quote()})));

    ASSERT_EQ(quotes.size(), 1u);
    EXPECT_EQ(quotes[0].symbol, "SPXW240327P04925000");
    EXPECT_EQ(quotes[0].value.timestamp, doc_quote_ns);
    EXPECT_EQ(quotes[0].value.timestamp, to_nanoseconds("2024-03-12T11:59:38.897261568Z"));
    EXPECT_EQ(quotes[0].value.bid_exchange, "C");
    EXPECT_DOUBLE_EQ(quotes[0].value.bid_price, 9.46);
    EXPECT_DOUBLE_EQ(quotes[0].value.bid_size, 53.);
    EXPECT_EQ(quotes[0].value.ask_exchange, "C");
    EXPECT_DOUBLE_EQ(quotes[0].value.ask_price, 9.66);
    EXPECT_DOUBLE_EQ(quotes[0].value.ask_size, 38.);
    EXPECT_EQ(quotes[0].value.conditions, std::vector<std::string>{"A"});
    EXPECT_NEAR(quotes[0].value.spread(), 0.20, 1e-9);
}

TEST(OptionStreamDecoding, HandEncodedFrameInAlpacasKeyOrder) {
    // Byte for byte from the MessagePack specification, with `T` first as Alpaca sends
    // it. nlohmann sorts keys, so every packed frame above puts `S` ahead of `T`; between
    // them the two cover both orders.
    std::string frame;
    const auto put = [&frame](std::initializer_list<int> bytes) {
        for (const int b : bytes) { frame.push_back(static_cast<char>(b)); }
    };
    const auto put_all = [&frame](const std::vector<std::uint8_t> &bytes) {
        frame.append(bytes.begin(), bytes.end());
    };
    const std::string symbol = "AAPL240315C00172500";

    put({0x91});                                    // array of 1
    put({0x87});                                    // map of 7
    put({0xA1, 'T', 0xA1, 't'});
    put({0xA1, 'S', 0xA0 | static_cast<int>(symbol.size())});
    frame += symbol;
    put({0xA1, 't', 0xD7, 0xFF});                   // fixext 8, type -1
    put_all(big_endian(((doc_trade_ns % 1'000'000'000ull) << 34)
                           | (doc_trade_ns / 1'000'000'000ull), 8));
    put({0xA1, 'p', 0xCB});                         // float 64
    put_all(big_endian(std::bit_cast<std::uint64_t>(2.84), 8));
    put({0xA1, 's', 0x01});                         // positive fixint
    put({0xA1, 'x', 0xA1, 'N'});
    put({0xA1, 'c', 0xA1, 'S'});

    testable_stream stream(dummy());
    std::vector<seen_trade> trades;
    stream.on_trade([&](const std::string &s, const trade &t) { trades.push_back({s, t}); });

    stream.feed(frame);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].symbol, symbol);
    EXPECT_EQ(trades[0].value.timestamp, doc_trade_ns);
    EXPECT_DOUBLE_EQ(trades[0].value.price, 2.84);
    EXPECT_DOUBLE_EQ(trades[0].value.size, 1.);
    EXPECT_EQ(trades[0].value.exchange, "N");
    EXPECT_EQ(trades[0].value.conditions, std::vector<std::string>{"S"});
}

TEST(OptionStreamDecoding, EveryMessageOfABatchIsDeliveredInOrder) {
    testable_stream stream(dummy());
    std::vector<std::string> order;
    stream.on_trade([&](const std::string &s, const trade &) { order.push_back("t:" + s); });
    stream.on_quote([&](const std::string &s, const quote &) { order.push_back("q:" + s); });

    json second_trade = doc_trade();
    second_trade["S"] = "AAPL240315P00172500";
    stream.feed(pack(json::array({doc_trade(), doc_quote(), second_trade})));

    EXPECT_EQ(order, (std::vector<std::string>{"t:AAPL240315C00172500",
                                               "q:SPXW240327P04925000",
                                               "t:AAPL240315P00172500"}));
}

TEST(OptionStreamDecoding, ABareMapIsAcceptedAsAFrame) {
    testable_stream stream(dummy());
    int trades = 0;
    stream.on_trade([&](const std::string &, const trade &) { ++trades; });

    stream.feed(pack(doc_trade()));

    EXPECT_EQ(trades, 1);
}

TEST(OptionStreamDecoding, NothingLeaksFromOneMessageIntoTheNext) {
    // The stream reuses one trade object, and the decoder one scratch message, for every
    // message — so a field the second message omits must not keep the first one's value.
    testable_stream stream(dummy());
    std::vector<seen_trade> trades;
    stream.on_trade([&](const std::string &s, const trade &t) { trades.push_back({s, t}); });

    const json sparse = {{"T", "t"}, {"S", "MSFT240315C00400000"}};
    stream.feed(pack(json::array({doc_trade(), sparse})));

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[1].symbol, "MSFT240315C00400000");
    EXPECT_EQ(trades[1].value.timestamp, 0u);
    EXPECT_DOUBLE_EQ(trades[1].value.price, 0.);
    EXPECT_DOUBLE_EQ(trades[1].value.size, 0.);
    EXPECT_TRUE(trades[1].value.exchange.empty());
    EXPECT_TRUE(trades[1].value.conditions.empty());
}

TEST(OptionStreamDecoding, ConditionsMayAlsoArriveAsAnArray) {
    // Options send one code as a bare string, stocks an array. The REST model accepts
    // both, so the stream does too.
    testable_stream stream(dummy());
    std::vector<seen_trade> trades;
    stream.on_trade([&](const std::string &s, const trade &t) { trades.push_back({s, t}); });

    json message = doc_trade();
    message["c"] = {"S", "I"};
    stream.feed(pack(json::array({message})));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].value.conditions, (std::vector<std::string>{"S", "I"}));
}

TEST(OptionStreamDecoding, NumbersAreAcceptedInAnyNumericEncoding) {
    // A whole-number price can be packed as an integer and a size as a float.
    testable_stream stream(dummy());
    std::vector<seen_trade> trades;
    stream.on_trade([&](const std::string &s, const trade &t) { trades.push_back({s, t}); });

    json message = doc_trade();
    message["p"] = 3;
    message["s"] = 25.0;
    stream.feed(pack(json::array({message})));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_DOUBLE_EQ(trades[0].value.price, 3.);
    EXPECT_DOUBLE_EQ(trades[0].value.size, 25.);
}

TEST(OptionStreamDecoding, UnknownAndNestedFieldsAreSkipped) {
    // A field Alpaca adds later must neither break decoding nor bleed into a known slot —
    // including a nested map that reuses a known key.
    testable_stream stream(dummy());
    std::vector<seen_trade> trades;
    int quotes = 0;
    stream.on_trade([&](const std::string &s, const trade &t) { trades.push_back({s, t}); });
    stream.on_quote([&](const std::string &, const quote &) { ++quotes; });

    json message = doc_trade();
    message["zz"] = {{"T", "q"}, {"S", "NESTED"}, {"p", 99.0}, {"list", {1, {{"x", "Q"}}}}};
    message["i"] = 12345;
    message["flag"] = true;
    message["nothing"] = nullptr;
    message["c"] = json::array({"S", json::array({"deep"}), {{"x", "Q"}}});
    stream.feed(pack(json::array({message})));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(quotes, 0);
    EXPECT_EQ(trades[0].symbol, "AAPL240315C00172500");
    EXPECT_DOUBLE_EQ(trades[0].value.price, 2.84);
    EXPECT_EQ(trades[0].value.exchange, "N");
    EXPECT_EQ(trades[0].value.conditions, std::vector<std::string>{"S"});
}

TEST(OptionStreamDecoding, MessagesWithoutAHandlerAreDropped) {
    testable_stream stream(dummy());

    EXPECT_NO_THROW(stream.feed(pack(json::array({doc_trade(), doc_quote()}))));
}

// ---------------------------------------------------------------------------
// Timestamps
// ---------------------------------------------------------------------------

class OptionStreamTimestamps : public ::testing::Test {
protected:
    /// Decodes one trade whose `t` is `timestamp` and returns the nanoseconds delivered.
    static std::uint64_t decode(const json &timestamp) {
        testable_stream stream(dummy());
        std::uint64_t seen = ~0ull;
        stream.on_trade([&](const std::string &, const trade &t) { seen = t.timestamp; });

        json message = doc_trade();
        message["t"] = timestamp;
        stream.feed(pack(json::array({message})));
        return seen;
    }
};

TEST_F(OptionStreamTimestamps, Timestamp32IsWholeSeconds) {
    EXPECT_EQ(decode(ext(big_endian(1'710'164'135ull, 4), timestamp_type)),
              1'710'164'135'000'000'000ull);
}

TEST_F(OptionStreamTimestamps, Timestamp64KeepsNanoseconds) {
    EXPECT_EQ(decode(timestamp64(doc_trade_ns)), doc_trade_ns);
    // The largest nanosecond field, to pin down the 30/34 bit split.
    EXPECT_EQ(decode(timestamp64(1'710'164'135'999'999'999ull)), 1'710'164'135'999'999'999ull);
}

TEST_F(OptionStreamTimestamps, Timestamp96IsNanosecondsThenSeconds) {
    auto payload = big_endian(133'122'560ull, 4);
    const auto seconds = big_endian(1'710'164'135ull, 8);
    payload.insert(payload.end(), seconds.begin(), seconds.end());

    EXPECT_EQ(decode(ext(payload, timestamp_type)), doc_trade_ns);
}

TEST_F(OptionStreamTimestamps, InstantsBeforeTheEpochCollapseToZero) {
    auto payload = big_endian(0, 4);
    const auto seconds = big_endian(static_cast<std::uint64_t>(-5ll), 8);
    payload.insert(payload.end(), seconds.begin(), seconds.end());

    EXPECT_EQ(decode(ext(payload, timestamp_type)), 0u);
}

TEST_F(OptionStreamTimestamps, Rfc3339TextIsAcceptedToo) {
    EXPECT_EQ(decode("2024-03-11T13:35:35.13312256Z"), doc_trade_ns);
}

TEST_F(OptionStreamTimestamps, ForeignExtensionsAreNotMistakenForTimestamps) {
    EXPECT_EQ(decode(ext(big_endian(1'710'164'135ull, 4), 5)), 0u);
    EXPECT_EQ(decode(ext(big_endian(1, 2), timestamp_type)), 0u);     // no such length
}

// ---------------------------------------------------------------------------
// Control messages
// ---------------------------------------------------------------------------

TEST(OptionStreamControl, AuthenticatedAcknowledgementMakesTheStreamReady) {
    testable_stream stream(dummy());
    int connected = 0;
    stream.on_connected([&] { ++connected; });

    stream.feed(pack(json::array({{{"T", "success"}, {"msg", "connected"}}})));
    EXPECT_FALSE(stream.is_ready()) << "the connect greeting is not the auth acknowledgement";

    stream.feed(pack(json::array({{{"T", "success"}, {"msg", "authenticated"}}})));
    EXPECT_TRUE(stream.is_ready());
    EXPECT_EQ(connected, 1);
}

TEST(OptionStreamControl, ErrorFramesReachTheErrorHandler) {
    testable_stream stream(dummy());
    std::vector<stream_error> errors;
    stream.on_error([&](const stream_error &e) { errors.push_back(e); });

    stream.feed(pack(json::array({{{"T", "error"}, {"code", 413},
        {"msg", "star subscription is not allowed for option quotes"}}})));

    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].code, 413);
    EXPECT_EQ(errors[0].message, "star subscription is not allowed for option quotes");
}

TEST(OptionStreamControl, SubscriptionEchoCarriesTheServersSet) {
    testable_stream stream(dummy());
    std::vector<subscriptions> echoes;
    stream.on_subscription([&](const subscriptions &s) { echoes.push_back(s); });

    stream.feed(pack(json::array({{{"T", "subscription"},
        {"trades", {"AAPL240315C00172500", "AAPL240315P00172500"}},
        {"quotes", {"SPXW240327P04925000"}}}})));

    ASSERT_EQ(echoes.size(), 1u);
    EXPECT_EQ(echoes[0].trades,
              (std::vector<std::string>{"AAPL240315C00172500", "AAPL240315P00172500"}));
    EXPECT_EQ(echoes[0].quotes, std::vector<std::string>{"SPXW240327P04925000"});
    EXPECT_TRUE(echoes[0].bars.empty());
}

TEST(OptionStreamControl, EmptySubscriptionEchoClearsBothLists) {
    testable_stream stream(dummy());
    std::vector<subscriptions> echoes;
    stream.on_subscription([&](const subscriptions &s) { echoes.push_back(s); });

    stream.feed(pack(json::array({{{"T", "subscription"},
        {"trades", json::array()}, {"quotes", json::array()}}})));

    ASSERT_EQ(echoes.size(), 1u);
    EXPECT_TRUE(echoes[0].empty());
}

TEST(OptionStreamControl, JsonTextFrameIsReportedAsItself) {
    // The option endpoints answer in MessagePack unless the client asks for JSON, which
    // this SDK never does. If a server ever answers in JSON anyway, its refusal has to
    // surface as Alpaca's own error, not as an undecodable binary frame. This is the
    // frame a JSON-mode connection sends back for our MessagePack auth.
    testable_stream stream(dummy());
    std::vector<stream_error> errors;
    stream.on_error([&](const stream_error &e) { errors.push_back(e); });

    stream.feed(R"([{"T":"error","code":400,"msg":"invalid syntax"}])");

    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].code, 400);
    EXPECT_EQ(errors[0].message, "invalid syntax");
}

// ---------------------------------------------------------------------------
// Frames as the server sends them
//
// Captured from wss://stream.data.alpaca.markets/v1beta1/indicative. Unlike the packed
// frames above these carry Alpaca's own key order and integer widths — the error code,
// for one, arrives as a full uint 32 rather than the shortest encoding.
// ---------------------------------------------------------------------------

TEST(OptionStreamCaptured, HandshakeFrames) {
    testable_stream stream(dummy());

    // [{"T":"success","msg":"connected"}]
    stream.feed(from_hex("9182a154a773756363657373a36d7367a9636f6e6e6563746564"));
    EXPECT_FALSE(stream.is_ready());

    // [{"T":"success","msg":"authenticated"}]
    stream.feed(from_hex("9182a154a773756363657373a36d7367ad61757468656e74696361746564"));
    EXPECT_TRUE(stream.is_ready());
}

TEST(OptionStreamCaptured, SubscriptionEcho) {
    testable_stream stream(dummy());
    std::vector<subscriptions> echoes;
    stream.on_subscription([&](const subscriptions &s) { echoes.push_back(s); });

    stream.feed(from_hex(
        "9183a154ac737562736372697074696f6ea674726164657392b34141504c32363132313843303032"
        "3530303030b2535059323631323138433030363530303030a671756f74657392b34141504c323631"
        "323138433030323530303030b2535059323631323138433030363530303030"));

    ASSERT_EQ(echoes.size(), 1u);
    const std::vector<std::string> both{"AAPL261218C00250000", "SPY261218C00650000"};
    EXPECT_EQ(echoes[0].trades, both);
    EXPECT_EQ(echoes[0].quotes, both);
}

TEST(OptionStreamCaptured, EchoOmitsAChannelThatBecameEmpty) {
    // After the last quote is unsubscribed the server leaves `quotes` out altogether
    // rather than sending an empty list.
    testable_stream stream(dummy());
    std::vector<subscriptions> echoes;
    stream.on_subscription([&](const subscriptions &s) { echoes.push_back(s); });

    stream.feed(from_hex(
        "9182a154ac737562736372697074696f6ea674726164657391b34141504c32363132313843303032"
        "3530303030"));

    ASSERT_EQ(echoes.size(), 1u);
    EXPECT_EQ(echoes[0].trades, std::vector<std::string>{"AAPL261218C00250000"});
    EXPECT_TRUE(echoes[0].quotes.empty());
}

TEST(OptionStreamCaptured, WildcardQuoteRefusal) {
    testable_stream stream(dummy());
    std::vector<stream_error> errors;
    stream.on_error([&](const stream_error &e) { errors.push_back(e); });

    stream.feed(from_hex(
        "9183a154a56572726f72a4636f6465ce0000019da36d7367d9327374617220737562736372697074"
        "696f6e206973206e6f7420616c6c6f77656420666f72206f7074696f6e2071756f746573"));

    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].code, 413);
    EXPECT_EQ(errors[0].message, "star subscription is not allowed for option quotes");
}

TEST(OptionStreamCaptured, TradeAndQuoteAsTheStreamingBackendPacksThem) {
    // The option feeds are silent outside market hours, so this frame comes from the
    // around-the-clock synthetic stock feed (`v2/test`, in MessagePack mode), which the
    // same backend serves. It pins down what a real data message looks like: a fixext 8
    // timestamp, float 64 prices, uint 32 sizes — plus fields no option message carries
    // (`i`, `z`, a second timestamp `r`) that the decoder has to step over.
    testable_stream stream(dummy());
    std::vector<seen_trade> trades;
    std::vector<seen_quote> quotes;
    stream.on_trade([&](const std::string &s, const trade &t) { trades.push_back({s, t}); });
    stream.on_quote([&](const std::string &s, const quote &q) { quotes.push_back({s, q}); });

    stream.feed(from_hex(
        "928aa154a174a153a846414b4550414341a169d30000000000000001a178a14ea170cb4060d1eb85"
        "1eb852a173ce00000003a16391a120a17aa141a174d7ff455034746acc53e1a172d7ff455035646a"
        "cc53e18ca154a171a153a846414b4550414341a26278a14fa26270cb4060bb3333333333a26273ce"
        "00000004a26178a152a26170cb4060f8a3d70a3d71a26173ce00000005a16391a152a17aa141a174"
        "d7ff45519c246acc53e1a172d7ff45519cec6acc53e1"));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].symbol, "FAKEPACA");
    EXPECT_EQ(trades[0].value.timestamp, 1'791'775'713'290'721'053ull);
    EXPECT_DOUBLE_EQ(trades[0].value.price, 134.56);
    EXPECT_DOUBLE_EQ(trades[0].value.size, 3.);
    EXPECT_EQ(trades[0].value.exchange, "N");
    EXPECT_EQ(trades[0].value.conditions, std::vector<std::string>{" "});

    ASSERT_EQ(quotes.size(), 1u);
    EXPECT_EQ(quotes[0].symbol, "FAKEPACA");
    EXPECT_EQ(quotes[0].value.timestamp, 1'791'775'713'290'744'073ull);
    EXPECT_EQ(quotes[0].value.bid_exchange, "O");
    EXPECT_DOUBLE_EQ(quotes[0].value.bid_price, 133.85);
    EXPECT_DOUBLE_EQ(quotes[0].value.bid_size, 4.);
    EXPECT_EQ(quotes[0].value.ask_exchange, "R");
    EXPECT_DOUBLE_EQ(quotes[0].value.ask_price, 135.77);
    EXPECT_DOUBLE_EQ(quotes[0].value.ask_size, 5.);
    EXPECT_EQ(quotes[0].value.conditions, std::vector<std::string>{"R"});
}

// ---------------------------------------------------------------------------
// Malformed input
// ---------------------------------------------------------------------------

TEST(OptionStreamMalformed, TruncatedFrameIsReportedAndEarlierMessagesStillArrive) {
    testable_stream stream(dummy());
    int trades = 0;
    std::vector<stream_error> errors;
    stream.on_trade([&](const std::string &, const trade &) { ++trades; });
    stream.on_error([&](const stream_error &e) { errors.push_back(e); });

    std::string frame = pack(json::array({doc_trade(), doc_trade()}));
    frame.resize(frame.size() - 3);
    stream.feed(frame);

    EXPECT_EQ(trades, 1);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].code, 0);
    EXPECT_EQ(errors[0].message.rfind("malformed stream frame", 0), 0u) << errors[0].message;
}

TEST(OptionStreamMalformed, InvalidByteIsReported) {
    testable_stream stream(dummy());
    std::vector<stream_error> errors;
    stream.on_error([&](const stream_error &e) { errors.push_back(e); });

    stream.feed(std::string("\x91\xC1", 2));    // 0xC1 is never used by MessagePack

    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].code, 0);
}

TEST(OptionStreamMalformed, AGoodFrameAfterABadOneDecodesNormally) {
    // The decoder keeps its parse state between frames, so a fault halfway through one
    // must not poison the next.
    testable_stream stream(dummy());
    std::vector<seen_trade> trades;
    stream.on_trade([&](const std::string &s, const trade &t) { trades.push_back({s, t}); });
    stream.on_error([](const stream_error &) {});

    std::string broken = pack(json::array({doc_trade()}));
    broken.resize(broken.size() / 2);
    stream.feed(broken);
    ASSERT_TRUE(trades.empty());

    stream.feed(pack(json::array({doc_trade()})));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].symbol, "AAPL240315C00172500");
    EXPECT_EQ(trades[0].value.timestamp, doc_trade_ns);
    EXPECT_DOUBLE_EQ(trades[0].value.price, 2.84);
}

}   // namespace alpaca::tests
