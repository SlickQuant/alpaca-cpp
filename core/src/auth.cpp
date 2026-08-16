// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/auth.hpp>
#include <alpaca/utils.hpp>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

namespace alpaca {

credentials credentials::from_env() {
    credentials c;
    c.api_key_id = get_env("APCA_API_KEY_ID");
    c.api_secret_key = get_env("APCA_API_SECRET_KEY");
    c.oauth_token = get_env("APCA_API_OAUTH_TOKEN");
    return c;
}

credentials credentials::from_env(environment env) {
    if (env == environment::paper) {
        credentials paper;
        paper.api_key_id = get_env("APCA_PAPER_API_KEY_ID");
        paper.api_secret_key = get_env("APCA_PAPER_API_SECRET_KEY");
        paper.oauth_token = get_env("APCA_PAPER_API_OAUTH_TOKEN");
        // Only use the paper pair when it is actually configured; otherwise fall back so
        // a single-account setup keeps working with just the unprefixed variables.
        if (!paper.empty()) {
            return paper;
        }
    }
    return from_env();
}

header_list credentials::trading_headers() const {
    if (uses_oauth()) {
        return {{"Authorization", "Bearer " + oauth_token}};
    }
    return {
        {"APCA-API-KEY-ID", api_key_id},
        {"APCA-API-SECRET-KEY", api_secret_key},
    };
}

header_list credentials::basic_auth_headers() const {
    if (uses_oauth()) {
        return {{"Authorization", "Bearer " + oauth_token}};
    }
    return {{"Authorization", "Basic " + base64_encode(api_key_id + ":" + api_secret_key)}};
}

std::string credentials::stream_auth_message(std::string_view action) const {
    nlohmann::json j;
    j["action"] = action;
    if (uses_oauth()) {
        // Documented OAuth form for the streams.
        j["key"] = "oauth";
        j["secret"] = oauth_token;
    }
    else {
        j["key"] = api_key_id;
        j["secret"] = api_secret_key;
    }
    return j.dump();
}

std::string base64_encode(std::string_view data) {
    if (data.empty()) {
        return {};
    }

    // EVP_EncodeBlock writes 4 output bytes per 3 input bytes, plus a NUL terminator.
    const size_t encoded_size = 4 * ((data.size() + 2) / 3);
    std::string out(encoded_size + 1, '\0');

    const int written = EVP_EncodeBlock(
        reinterpret_cast<unsigned char *>(out.data()),
        reinterpret_cast<const unsigned char *>(data.data()),
        static_cast<int>(data.size()));

    if (written < 0) {
        return {};
    }
    out.resize(static_cast<size_t>(written));
    return out;
}

}   // namespace alpaca
