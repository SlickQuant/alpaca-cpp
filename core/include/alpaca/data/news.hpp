// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <alpaca/common.hpp>
#include <alpaca/utils.hpp>

using json = nlohmann::json;

namespace alpaca {

/// An image attached to a news article, in one of three sizes.
struct news_image {
    std::string size;   ///< "thumb", "small" or "large"
    std::string url;
};

inline void from_json(const json &j, news_image &i) {
    VARIABLE_FROM_JSON(j, i, size);
    VARIABLE_FROM_JSON(j, i, url);
}

/// A news article. `GET /v1beta1/news`.
/// Unlike the price endpoints, this one uses spelled-out field names on the wire.
struct news_article {
    int64_t id = 0;
    std::string headline;
    std::string author;
    std::string summary;
    /// Only populated when the request set `include_content`; may contain HTML.
    std::string content;
    std::string url;
    std::string source;
    std::vector<std::string> symbols;
    std::vector<news_image> images;
    uint64_t created_at = 0;    ///< nanoseconds since the Unix epoch
    uint64_t updated_at = 0;
};

inline void from_json(const json &j, news_article &n) {
    INT_FROM_JSON(j, n, id);
    VARIABLE_FROM_JSON(j, n, headline);
    VARIABLE_FROM_JSON(j, n, author);
    VARIABLE_FROM_JSON(j, n, summary);
    VARIABLE_FROM_JSON(j, n, content);
    VARIABLE_FROM_JSON(j, n, url);
    VARIABLE_FROM_JSON(j, n, source);
    VARIABLE_FROM_JSON(j, n, symbols);
    TIMESTAMP_FROM_JSON(j, n, created_at);
    TIMESTAMP_FROM_JSON(j, n, updated_at);
    if (j.contains("images") && j["images"].is_array()) {
        n.images = j["images"].get<std::vector<news_image>>();
    }
}

/// One page of `GET /v1beta1/news`.
struct news_page {
    std::vector<news_article> news;
    std::string next_page_token;
};

inline void from_json(const json &j, news_page &p) {
    if (j.contains("news") && j["news"].is_array()) {
        p.news = j["news"].get<std::vector<news_article>>();
    }
    if (j.contains("next_page_token") && !j["next_page_token"].is_null()) {
        p.next_page_token = j["next_page_token"].get<std::string>();
    }
}

/// Query parameters for `GET /v1beta1/news`.
struct news_query {
    std::vector<std::string> symbols;
    std::optional<std::string> start;   ///< RFC-3339 or YYYY-MM-DD
    std::optional<std::string> end;
    /// 1-50, default 10.
    std::optional<uint32_t> limit;
    std::optional<alpaca::sort_direction> sort;
    std::optional<bool> include_content;
    std::optional<bool> exclude_contentless;
    std::optional<std::string> page_token;

    query_builder build() const {
        query_builder q;
        q.add("symbols", symbols);
        q.add("start", start);
        q.add("end", end);
        q.add("limit", limit);
        q.add("sort", sort);
        q.add("include_content", include_content);
        q.add("exclude_contentless", exclude_contentless);
        q.add("page_token", page_token);
        return q;
    }
};

}   // namespace alpaca
