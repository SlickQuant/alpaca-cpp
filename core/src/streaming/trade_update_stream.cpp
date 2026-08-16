// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/streaming/trade_update_stream.hpp>

namespace alpaca {

trade_update_stream::trade_update_stream(credentials creds, environment env)
    : detail::stream_base(std::string(trade_stream_url(env)),
                          credentials::resolve(std::move(creds), env))
    , env_(env)
{}

void trade_update_stream::on_ready() {
    // Unlike the data streams there is nothing to configure — the only channel this
    // socket carries is trade_updates — but it still has to be asked for explicitly,
    // and asked for again after every reconnect.
    send(R"({"action":"listen","data":{"streams":["trade_updates"]}})");
}

void trade_update_stream::handle_payload(const json &payload) {
    if (!payload.is_object()) {
        return;
    }

    const auto stream_it = payload.find("stream");
    if (stream_it == payload.end() || !stream_it->is_string()) {
        return;
    }
    const std::string &channel = stream_it->get_ref<const std::string &>();

    const auto data_it = payload.find("data");
    const bool has_data = data_it != payload.end() && data_it->is_object();

    if (channel == "authorization") {
        // {"stream":"authorization","data":{"status":"authorized","action":"authenticate"}}
        std::string status;
        if (has_data) {
            const auto status_it = data_it->find("status");
            if (status_it != data_it->end() && status_it->is_string()) {
                status = status_it->get_ref<const std::string &>();
            }
        }

        if (status == "authorized") {
            mark_ready();
        }
        else {
            // The socket stays open after a rejected auth, so without this the caller
            // would sit in connect() until the timeout with no clue why.
            report_error({401, status.empty() ? "authorization refused" : status});
        }
        return;
    }

    if (channel == "listening") {
        return;     // echo of the listen request; nothing to do
    }

    if (channel == "trade_updates" && has_data && on_trade_update_) {
        trade_update update;
        from_json(*data_it, update);
        on_trade_update_(update);
    }
}

}   // namespace alpaca
