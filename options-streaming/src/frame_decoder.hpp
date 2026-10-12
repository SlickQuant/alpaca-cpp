// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <msgpack/null_visitor.hpp>
#include <msgpack/parse.hpp>
#include <msgpack/unpack.hpp>   // parse.hpp leans on helpers it does not include itself

#include <alpaca/utils.hpp>

namespace alpaca::detail {

/// One message of a frame, held as views into the frame's own bytes.
///
/// Every message the option streams send is a flat map, so one struct with a slot per
/// known key covers trades, quotes and the control messages alike. Nothing is copied
/// while a frame is being decoded: the strings point into the websocket read buffer and
/// are only valid until the sink returns.
struct raw_message {
    std::string_view type;                      ///< `T`
    std::string_view symbol;                    ///< `S`
    std::string_view text;                      ///< `msg`
    std::string_view exchange;                  ///< `x`
    std::string_view bid_exchange;              ///< `bx`
    std::string_view ask_exchange;              ///< `ax`
    uint64_t timestamp = 0;                     ///< `t` — nanoseconds since the Unix epoch
    int64_t code = 0;                           ///< `code`
    double price = 0.;                          ///< `p`
    double size = 0.;                           ///< `s`
    double bid_price = 0.;                      ///< `bp`
    double bid_size = 0.;                       ///< `bs`
    double ask_price = 0.;                      ///< `ap`
    double ask_size = 0.;                       ///< `as`
    std::vector<std::string_view> conditions;   ///< `c`
    std::vector<std::string_view> trades;       ///< `trades` of a subscription echo
    std::vector<std::string_view> quotes;       ///< `quotes` of a subscription echo

    /// Resets every slot but keeps the vectors' storage, so a steady stream of messages
    /// settles into never allocating.
    void clear() noexcept {
        type = symbol = text = exchange = bid_exchange = ask_exchange = {};
        timestamp = 0;
        code = 0;
        price = size = bid_price = bid_size = ask_price = ask_size = 0.;
        conditions.clear();
        trades.clear();
        quotes.clear();
    }
};

enum class message_field : uint8_t {
    unknown,
    type,
    symbol,
    text,
    exchange,
    bid_exchange,
    ask_exchange,
    timestamp,
    code,
    price,
    size,
    bid_price,
    bid_size,
    ask_price,
    ask_size,
    conditions,
    trades,
    quotes,
};

/// Maps a wire key to its slot. Keys are one or two characters on the hot messages, so
/// this is a switch on length and bytes rather than a chain of string comparisons.
inline message_field field_of(std::string_view key) noexcept {
    switch (key.size()) {
    case 1:
        switch (key[0]) {
        case 'T': return message_field::type;
        case 'S': return message_field::symbol;
        case 't': return message_field::timestamp;
        case 'p': return message_field::price;
        case 's': return message_field::size;
        case 'x': return message_field::exchange;
        case 'c': return message_field::conditions;
        default: break;
        }
        break;
    case 2:
        if (key[0] == 'b') {
            switch (key[1]) {
            case 'x': return message_field::bid_exchange;
            case 'p': return message_field::bid_price;
            case 's': return message_field::bid_size;
            default: break;
            }
        }
        else if (key[0] == 'a') {
            switch (key[1]) {
            case 'x': return message_field::ask_exchange;
            case 'p': return message_field::ask_price;
            case 's': return message_field::ask_size;
            default: break;
            }
        }
        break;
    case 3:
        if (key == "msg") { return message_field::text; }
        break;
    case 4:
        if (key == "code") { return message_field::code; }
        break;
    case 6:
        if (key == "trades") { return message_field::trades; }
        if (key == "quotes") { return message_field::quotes; }
        break;
    default:
        break;
    }
    return message_field::unknown;
}

inline uint64_t load_big_endian(const char *data, std::size_t bytes) noexcept {
    uint64_t value = 0;
    for (std::size_t i = 0; i < bytes; ++i) {
        value = (value << 8) | static_cast<unsigned char>(data[i]);
    }
    return value;
}

/// Converts a MessagePack timestamp extension to nanoseconds since the Unix epoch.
///
/// `data` points at the extension type byte and `size` counts it, which is how the
/// msgpack visitor hands extensions over. The type must be -1, and the payload length
/// selects one of the three encodings the specification defines. Anything else — and any
/// instant before the epoch, which the unsigned result cannot hold — yields 0.
inline uint64_t timestamp_from_ext(const char *data, std::size_t size) noexcept {
    constexpr uint64_t nanos_per_second = 1'000'000'000ull;

    if (size == 0 || static_cast<signed char>(data[0]) != -1) {
        return 0;
    }

    const char *payload = data + 1;
    switch (size - 1) {
    case 4:     // timestamp 32: seconds
        return load_big_endian(payload, 4) * nanos_per_second;
    case 8: {   // timestamp 64: 30 bits of nanoseconds above 34 bits of seconds
        const uint64_t packed = load_big_endian(payload, 8);
        return (packed & 0x3'ffff'ffffull) * nanos_per_second + (packed >> 34);
    }
    case 12: {  // timestamp 96: 32 bits of nanoseconds, then signed 64-bit seconds
        const auto seconds = static_cast<int64_t>(load_big_endian(payload + 4, 8));
        return seconds < 0
            ? 0
            : static_cast<uint64_t>(seconds) * nanos_per_second + load_big_endian(payload, 4);
    }
    default:
        return 0;
    }
}

/// SAX visitor that turns a frame into a sequence of `raw_message`s.
///
/// A frame is an array of flat maps. Any map that opens outside a message is treated as
/// one, which also accepts a bare map as a frame. Values the SDK has no slot for — unknown
/// keys, nested containers — are walked over and dropped, so a field Alpaca adds later
/// costs nothing and breaks nothing.
template <typename Sink>
class message_visitor : public msgpack::null_visitor {
public:
    explicit message_visitor(Sink &sink) noexcept : sink_(sink) {}

    /// Called by the parse context before each frame.
    void init() noexcept {
        depth_ = 0;
        message_depth_ = 0;
        field_ = message_field::unknown;
        list_ = nullptr;
        expecting_key_ = false;
    }

    bool visit_positive_integer(uint64_t v) { number(static_cast<double>(v)); return true; }
    bool visit_negative_integer(int64_t v) { number(static_cast<double>(v)); return true; }
    bool visit_float32(float v) { number(v); return true; }
    bool visit_float64(double v) { number(v); return true; }

    bool visit_str(const char *v, uint32_t size) {
        const std::string_view value(v, size);
        if (expecting_key_) {
            field_ = field_of(value);
        }
        else if (in_list()) {
            list_->push_back(value);
        }
        else if (at_value()) {
            string(value);
        }
        return true;
    }

    bool visit_ext(const char *v, std::size_t size) {
        if (at_value() && field_ == message_field::timestamp) {
            message_.timestamp = timestamp_from_ext(v, size);
        }
        return true;
    }

    bool start_array(uint32_t /*num_elements*/) {
        if (at_value()) {
            list_ = list_for(field_);
            if (list_ != nullptr) {
                list_->clear();
            }
        }
        ++depth_;
        return true;
    }

    bool end_array() {
        if (--depth_ == message_depth_) {
            list_ = nullptr;
        }
        return true;
    }

    bool start_map(uint32_t /*num_kv_pairs*/) {
        ++depth_;
        if (message_depth_ == 0) {
            message_depth_ = depth_;
            message_.clear();
        }
        return true;
    }

    bool start_map_key() {
        if (depth_ == message_depth_) {
            // Reset here rather than on the key itself: a non-string key never reaches
            // visit_str, and its value must not land in the previous key's slot.
            field_ = message_field::unknown;
            expecting_key_ = true;
        }
        return true;
    }

    bool end_map_key() {
        expecting_key_ = false;
        return true;
    }

    bool end_map() {
        if (depth_ == message_depth_) {
            message_depth_ = 0;
            sink_.on_message(message_);
        }
        --depth_;
        return true;
    }

private:
    /// A scalar here is the value of the current key of the message being built.
    bool at_value() const noexcept {
        return message_depth_ != 0 && depth_ == message_depth_ && !expecting_key_;
    }

    /// A scalar here is an element of a list this decoder keeps.
    bool in_list() const noexcept {
        return list_ != nullptr && depth_ == message_depth_ + 1;
    }

    std::vector<std::string_view>* list_for(message_field field) noexcept {
        switch (field) {
        case message_field::conditions: return &message_.conditions;
        case message_field::trades:     return &message_.trades;
        case message_field::quotes:     return &message_.quotes;
        default:                        return nullptr;
        }
    }

    void string(std::string_view value) {
        switch (field_) {
        case message_field::type:         message_.type = value; break;
        case message_field::symbol:       message_.symbol = value; break;
        case message_field::text:         message_.text = value; break;
        case message_field::exchange:     message_.exchange = value; break;
        case message_field::bid_exchange: message_.bid_exchange = value; break;
        case message_field::ask_exchange: message_.ask_exchange = value; break;
        // Options carry one condition code as a bare string; stocks send an array.
        case message_field::conditions:   message_.conditions.assign(1, value); break;
        // The documented encoding is the timestamp extension, but RFC-3339 text is what
        // the same field looks like on every JSON stream, so accept it too.
        case message_field::timestamp:    message_.timestamp = to_nanoseconds(value); break;
        default: break;
        }
    }

    /// Prices are floats and sizes are integers on the wire, but a whole-number price may
    /// be packed as an integer, so every numeric encoding is accepted for every slot.
    void number(double value) noexcept {
        if (!at_value()) {
            return;
        }
        switch (field_) {
        case message_field::price:     message_.price = value; break;
        case message_field::size:      message_.size = value; break;
        case message_field::bid_price: message_.bid_price = value; break;
        case message_field::bid_size:  message_.bid_size = value; break;
        case message_field::ask_price: message_.ask_price = value; break;
        case message_field::ask_size:  message_.ask_size = value; break;
        case message_field::code:      message_.code = static_cast<int64_t>(value); break;
        default: break;
        }
    }

    Sink &sink_;
    raw_message message_;
    uint32_t depth_ = 0;
    uint32_t message_depth_ = 0;    ///< container depth of the open message; 0 when none
    message_field field_ = message_field::unknown;
    std::vector<std::string_view> *list_ = nullptr;
    bool expecting_key_ = false;
};

/// Decodes MessagePack frames into `raw_message`s delivered to `Sink::on_message`.
///
/// The parse context is a member rather than a local because msgpack's reserves its
/// container stack on construction; `msgpack::parse()` builds a fresh one per call, which
/// would put a heap allocation on every frame of the busiest feed in the market.
template <typename Sink>
class frame_decoder {
public:
    explicit frame_decoder(Sink &sink) : parser_(sink) {}

    /// Returns false when `data` is not one well-formed MessagePack object. Messages that
    /// completed before the fault have already been delivered. Throws only what the sink
    /// throws, plus `msgpack::unpack_error` on input msgpack refuses outright.
    bool decode(const char *data, std::size_t len) {
        parser_.init();
        std::size_t offset = 0;
        return parser_.execute(data, len, offset) == msgpack::PARSE_SUCCESS;
    }

private:
    struct parser : msgpack::detail::context<parser> {
        explicit parser(Sink &sink) noexcept : visitor_(sink) {}

        message_visitor<Sink>& visitor() noexcept { return visitor_; }

        message_visitor<Sink> visitor_;
    };

    parser parser_;
};

}   // namespace alpaca::detail
