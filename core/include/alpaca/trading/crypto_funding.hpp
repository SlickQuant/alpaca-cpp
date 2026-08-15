// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <alpaca/utils.hpp>

using json = nlohmann::json;

namespace alpaca {

/// An on-chain deposit address. `GET /v2/wallets`.
struct crypto_wallet {
    std::string address;
    std::string chain;
    uint64_t created_at = 0;    ///< nanoseconds since the Unix epoch
};

inline void from_json(const json &j, crypto_wallet &w) {
    VARIABLE_FROM_JSON(j, w, address);
    VARIABLE_FROM_JSON(j, w, chain);
    TIMESTAMP_FROM_JSON(j, w, created_at);
}

/// An on-chain deposit or withdrawal. `GET /v2/wallets/transfers`.
struct crypto_transfer {
    std::string id;
    std::string tx_hash;
    std::string direction;      ///< "INCOMING" or "OUTGOING"
    std::string status;         ///< "PROCESSING", "FAILED", "COMPLETE"
    std::string chain;
    std::string asset;
    std::string from_address;
    std::string to_address;
    double amount = 0.;
    double usd_value = 0.;
    double fees = 0.;
    double network_fee = 0.;
    uint64_t created_at = 0;
};

inline void from_json(const json &j, crypto_transfer &t) {
    VARIABLE_FROM_JSON(j, t, id);
    VARIABLE_FROM_JSON(j, t, tx_hash);
    VARIABLE_FROM_JSON(j, t, direction);
    VARIABLE_FROM_JSON(j, t, status);
    VARIABLE_FROM_JSON(j, t, chain);
    VARIABLE_FROM_JSON(j, t, asset);
    VARIABLE_FROM_JSON(j, t, from_address);
    VARIABLE_FROM_JSON(j, t, to_address);
    DOUBLE_FROM_JSON(j, t, amount);
    DOUBLE_FROM_JSON(j, t, usd_value);
    DOUBLE_FROM_JSON(j, t, fees);
    DOUBLE_FROM_JSON(j, t, network_fee);
    TIMESTAMP_FROM_JSON(j, t, created_at);
}

/// A destination address approved for withdrawals. `GET /v2/wallets/whitelists`.
struct whitelisted_address {
    std::string id;
    std::string chain;
    std::string asset;
    std::string address;
    std::string status;
    uint64_t created_at = 0;
};

inline void from_json(const json &j, whitelisted_address &a) {
    VARIABLE_FROM_JSON(j, a, id);
    VARIABLE_FROM_JSON(j, a, chain);
    VARIABLE_FROM_JSON(j, a, asset);
    VARIABLE_FROM_JSON(j, a, address);
    VARIABLE_FROM_JSON(j, a, status);
    TIMESTAMP_FROM_JSON(j, a, created_at);
}

/// Estimated network fee for a withdrawal. `GET /v2/wallets/fees/estimate`.
struct crypto_transfer_estimate {
    double fee = 0.;
};

inline void from_json(const json &j, crypto_transfer_estimate &e) {
    DOUBLE_FROM_JSON(j, e, fee);
}

/// Body for `POST /v2/wallets/transfers` (withdrawal request).
struct crypto_withdrawal_request {
    std::string amount;
    std::string address;
    std::string asset;

    json to_json() const {
        return json{{"amount", amount}, {"address", address}, {"asset", asset}};
    }
};

/// Body for `POST /v2/wallets/whitelists`.
struct whitelisted_address_request {
    std::string address;
    std::string asset;
    std::optional<std::string> chain;

    json to_json() const {
        json j{{"address", address}, {"asset", asset}};
        if (chain) {
            j["chain"] = *chain;
        }
        return j;
    }
};

}   // namespace alpaca
