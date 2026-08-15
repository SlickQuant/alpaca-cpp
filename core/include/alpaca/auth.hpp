// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace alpaca {

using header_list = std::vector<std::pair<std::string, std::string>>;

/// Alpaca API credentials.
///
/// Alpaca accepts three authentication schemes and the right one depends on the API:
///   * Trading and Market Data — `APCA-API-KEY-ID` / `APCA-API-SECRET-KEY` headers
///   * Broker API             — HTTP Basic with the key/secret pair
///   * OAuth (either)         — `Authorization: Bearer <token>`
///
/// All three live here so the broker library needs no authentication code of its own.
struct credentials {
    std::string api_key_id;
    std::string api_secret_key;
    /// When set, takes precedence over the key/secret pair.
    std::string oauth_token;

    credentials() = default;

    credentials(std::string key_id, std::string secret_key)
        : api_key_id(std::move(key_id))
        , api_secret_key(std::move(secret_key))
    {}

    /// Builds credentials from `APCA_API_KEY_ID` / `APCA_API_SECRET_KEY`, falling back to
    /// `APCA_API_OAUTH_TOKEN`. These are the same names the official SDKs use.
    static credentials from_env();

    /// Builds OAuth credentials from a bearer token.
    static credentials from_oauth_token(std::string token) {
        credentials c;
        c.oauth_token = std::move(token);
        return c;
    }

    bool empty() const noexcept {
        return oauth_token.empty() && (api_key_id.empty() || api_secret_key.empty());
    }

    bool uses_oauth() const noexcept { return !oauth_token.empty(); }

    /// Headers for the Trading and Market Data APIs.
    header_list trading_headers() const;

    /// Headers for the Broker API (HTTP Basic).
    header_list basic_auth_headers() const;

    /// Websocket auth payload for the market data streams and the trade-updates stream.
    /// OAuth uses the documented `{"key":"oauth","secret":"<token>"}` form.
    std::string stream_auth_message(std::string_view action = "auth") const;
};

/// Base64-encodes arbitrary bytes (used for Broker API HTTP Basic auth).
std::string base64_encode(std::string_view data);

}   // namespace alpaca
