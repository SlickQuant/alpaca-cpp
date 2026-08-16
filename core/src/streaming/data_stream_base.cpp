// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/streaming/data_stream_base.hpp>

namespace alpaca::detail {

const std::string& symbol_of(const json &message) {
    static const std::string none;
    const auto it = message.find("S");
    return (it != message.end() && it->is_string()) ? it->get_ref<const std::string &>() : none;
}

std::map<std::string, std::set<std::string>, std::less<>> data_stream_base::pending_channels() const {
    std::lock_guard lock(subs_mutex_);
    return channels_;
}

std::string data_stream_base::build_frame(std::string_view action) const {
    json frame;
    frame["action"] = action;
    for (const auto &[channel, symbols] : channels_) {
        if (!symbols.empty()) {
            frame[channel] = symbols;
        }
    }
    return frame.dump();
}

void data_stream_base::subscribe(std::string_view channel,
                                 const std::vector<std::string> &symbols) {
    if (symbols.empty()) {
        return;
    }

    std::string payload;
    {
        std::lock_guard lock(subs_mutex_);
        auto &set = channels_[std::string(channel)];
        set.insert(symbols.begin(), symbols.end());

        // Send only the delta, not the accumulated set: Alpaca treats a subscribe as
        // additive, so resending everything would be pure wasted bandwidth on a stream
        // that is already carrying a thousand symbols.
        json frame;
        frame["action"] = "subscribe";
        frame[std::string(channel)] = symbols;
        payload = frame.dump();
    }

    if (is_ready()) {
        send(payload);
    }
}

void data_stream_base::unsubscribe(std::string_view channel,
                                   const std::vector<std::string> &symbols) {
    if (symbols.empty()) {
        return;
    }

    std::string payload;
    {
        std::lock_guard lock(subs_mutex_);
        const auto it = channels_.find(channel);
        if (it != channels_.end()) {
            for (const auto &symbol : symbols) {
                it->second.erase(symbol);
            }
        }

        json frame;
        frame["action"] = "unsubscribe";
        frame[std::string(channel)] = symbols;
        payload = frame.dump();
    }

    if (is_ready()) {
        send(payload);
    }
}

void data_stream_base::on_ready() {
    std::string payload;
    {
        std::lock_guard lock(subs_mutex_);
        bool any = false;
        for (const auto &[channel, symbols] : channels_) {
            if (!symbols.empty()) {
                any = true;
                break;
            }
        }
        if (!any) {
            return;
        }
        payload = build_frame("subscribe");
    }
    send(payload);
}

void data_stream_base::handle_payload(const json &payload) {
    if (!payload.is_object()) {
        return;
    }

    const auto type_it = payload.find("T");
    if (type_it == payload.end() || !type_it->is_string()) {
        return;
    }
    const std::string &type = type_it->get_ref<const std::string &>();

    if (type == "success") {
        // Two successes arrive per session: "connected" then "authenticated". Only the
        // second means the session can carry subscriptions.
        const auto msg = payload.find("msg");
        if (msg != payload.end() && msg->is_string()
            && msg->get_ref<const std::string &>() == "authenticated") {
            mark_ready();
        }
        return;
    }

    if (type == "error") {
        stream_error error;
        from_json(payload, error);
        report_error(error);
        return;
    }

    if (type == "subscription") {
        if (on_subscription_) {
            subscriptions subs;
            from_json(payload, subs);
            on_subscription_(subs);
        }
        return;
    }

    dispatch(type, payload);
}

}   // namespace alpaca::detail
