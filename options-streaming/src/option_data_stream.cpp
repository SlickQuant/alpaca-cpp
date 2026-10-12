// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/streaming/option_data_stream.hpp>

#include <format>

#include "frame_decoder.hpp"

namespace alpaca {

namespace {

std::string option_stream_url(option_feed feed, environment env) {
    return std::format("{}/v1beta1/{}", data_stream_root(env), to_string_view(feed));
}

/// Copies views into strings that are reused from one message to the next, so the
/// vector and its elements keep their storage.
void assign_conditions(std::vector<std::string> &out, const std::vector<std::string_view> &in) {
    out.resize(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        out[i].assign(in[i]);
    }
}

}   // namespace

/// Owns the frame decoder and the objects handed to the handlers.
///
/// Only the websocket service thread touches this, one frame at a time, which is what
/// makes it safe to reuse a single `trade`, `quote` and symbol string for every message.
struct option_data_stream::decoder {
    explicit decoder(option_data_stream &stream) : stream_(stream), frames_(*this) {}

    bool decode(const char *data, std::size_t len) { return frames_.decode(data, len); }

    void on_message(const detail::raw_message &m) {
        // Single-character types, exactly as on the JSON streams: everything that is not
        // a trade or a quote is control traffic.
        if (m.type.size() == 1) {
            switch (m.type.front()) {
            case 't': on_trade(m); return;
            case 'q': on_quote(m); return;
            default: break;
            }
        }
        on_control(m);
    }

private:
    void on_trade(const detail::raw_message &m) {
        if (!stream_.on_trade_) {
            return;
        }
        symbol_.assign(m.symbol);
        trade_.timestamp = m.timestamp;
        trade_.price = m.price;
        trade_.size = m.size;
        trade_.exchange.assign(m.exchange);
        assign_conditions(trade_.conditions, m.conditions);
        stream_.on_trade_(symbol_, trade_);
    }

    void on_quote(const detail::raw_message &m) {
        if (!stream_.on_quote_) {
            return;
        }
        symbol_.assign(m.symbol);
        quote_.timestamp = m.timestamp;
        quote_.bid_price = m.bid_price;
        quote_.bid_size = m.bid_size;
        quote_.ask_price = m.ask_price;
        quote_.ask_size = m.ask_size;
        quote_.bid_exchange.assign(m.bid_exchange);
        quote_.ask_exchange.assign(m.ask_exchange);
        assign_conditions(quote_.conditions, m.conditions);
        stream_.on_quote_(symbol_, quote_);
    }

    /// Control messages are a handful per session, so they are rebuilt as JSON and given
    /// to the base class. The handshake, error and subscription handling then stays in
    /// one place for every market data stream instead of being repeated here.
    void on_control(const detail::raw_message &m) {
        json payload;
        payload["T"] = m.type;
        payload["msg"] = m.text;
        payload["code"] = m.code;
        payload["trades"] = m.trades;
        payload["quotes"] = m.quotes;
        stream_.handle_payload(payload);
    }

    option_data_stream &stream_;
    detail::frame_decoder<decoder> frames_;
    std::string symbol_;
    trade trade_;
    quote quote_;
};

option_data_stream::option_data_stream(credentials creds, option_feed feed, environment env)
    : detail::data_stream_base(option_stream_url(feed, env),
                               credentials::resolve(std::move(creds), env),
                               detail::frame_type::binary)
    , feed_(feed)
    , decoder_(std::make_unique<decoder>(*this))
{}

option_data_stream::~option_data_stream() {
    shutdown();
}

void option_data_stream::handle_frame(const char *data, std::size_t len) {
    // The official SDKs ask for this codec with a `Content-Type: application/msgpack`
    // header on the websocket upgrade. slick-net cannot add a header to that request, and
    // does not have to: with no Content-Type at all the option endpoints already answer in
    // MessagePack, and only an explicit `application/json` switches them (checked against
    // both feeds, October 2026). Should that default ever change, the server would answer
    // in JSON text instead — and every MessagePack array or map starts with a byte of 0x80
    // or above, which JSON text never does. Such a frame goes to the JSON path, so the
    // refusal Alpaca sends is reported as itself rather than as an undecodable frame.
    if (static_cast<unsigned char>(data[0]) < 0x80) {
        data_stream_base::handle_frame(data, len);
        return;
    }

    bool decoded = false;
    try {
        decoded = decoder_->decode(data, len);
    }
    catch (const msgpack::unpack_error &e) {
        report_error({0, std::string("malformed stream frame: ") + e.what()});
        return;
    }
    if (!decoded) {
        report_error({0, "malformed stream frame: truncated or invalid MessagePack"});
    }
}

std::string option_data_stream::encode(const json &frame) const {
    std::string out;
    json::to_msgpack(frame, out);
    return out;
}

std::string option_data_stream::auth_frame() const {
    return encode(json::parse(creds().stream_auth_message()));
}

void option_data_stream::dispatch(std::string_view, const json &) {
    // Trades and quotes never come this way — they are delivered straight from the
    // decoder. What does arrive is a control message type the SDK has no handling for.
}

}   // namespace alpaca
