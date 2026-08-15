// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline unit tests for error mapping and auth header construction.

#include <gtest/gtest.h>

#include <string>

#include <alpaca/auth.hpp>
#include <alpaca/detail/request_context.hpp>
#include <alpaca/error.hpp>

namespace alpaca::tests {

// ---------------------------------------------------------------------------
// api_error
// ---------------------------------------------------------------------------

TEST(ApiError, ParsesAlpacaCodeAndMessage) {
    // The canonical Alpaca error body.
    const std::string body = R"({"code":40010001,"message":"invalid symbol"})";
    try {
        detail::throw_api_error(422, "Unprocessable Entity", body);
    }
    catch (const api_error &e) {
        EXPECT_EQ(e.http_status, 422u);
        EXPECT_EQ(e.code, 40010001);
        EXPECT_EQ(e.message, "invalid symbol");
        EXPECT_EQ(e.body, body);
        EXPECT_NE(std::string(e.what()).find("invalid symbol"), std::string::npos);
        EXPECT_NE(std::string(e.what()).find("40010001"), std::string::npos);
    }
}

TEST(ApiError, HandlesMessageOnlyBody) {
    try {
        detail::throw_api_error(404, "Not Found", R"({"message":"order not found"})");
    }
    catch (const api_error &e) {
        EXPECT_EQ(e.http_status, 404u);
        EXPECT_EQ(e.code, 0);
        EXPECT_EQ(e.message, "order not found");
        EXPECT_TRUE(e.is_not_found());
    }
}

TEST(ApiError, FallsBackToReasonForNonJsonBody) {
    try {
        detail::throw_api_error(502, "Bad Gateway", "<html>upstream failed</html>");
    }
    catch (const api_error &e) {
        EXPECT_EQ(e.http_status, 502u);
        EXPECT_EQ(e.message, "Bad Gateway");
        EXPECT_EQ(e.body, "<html>upstream failed</html>");
    }
}

TEST(ApiError, FallsBackToBodyWhenReasonIsEmpty) {
    try {
        detail::throw_api_error(500, "", "internal failure");
    }
    catch (const api_error &e) {
        EXPECT_EQ(e.message, "internal failure");
    }
}

TEST(ApiError, ClassifiesStatuses) {
    const auto make = [](uint32_t status) { return api_error(status, 0, "", ""); };

    EXPECT_TRUE(make(401).is_unauthorized());
    EXPECT_TRUE(make(403).is_unauthorized());
    EXPECT_TRUE(make(404).is_not_found());
    EXPECT_TRUE(make(429).is_rate_limited());

    EXPECT_TRUE(make(429).is_retryable());
    EXPECT_TRUE(make(500).is_retryable());
    EXPECT_TRUE(make(503).is_retryable());
    EXPECT_FALSE(make(422).is_retryable());
    EXPECT_FALSE(make(404).is_retryable());
}

TEST(ApiError, IsCatchableAsStdException) {
    try {
        detail::throw_api_error(401, "Unauthorized", R"({"message":"access key verification failed"})");
    }
    catch (const std::exception &e) {
        EXPECT_NE(std::string(e.what()).find("401"), std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// Response body parsing
// ---------------------------------------------------------------------------

TEST(ParseBody, EmptyBodyBecomesNullRatherThanThrowing) {
    // 204 No Content is a normal outcome for DELETE /v2/orders/{id}.
    EXPECT_TRUE(detail::parse_body("").is_null());
    EXPECT_TRUE(detail::parse_body("   \r\n ").is_null());
}

TEST(ParseBody, ParsesObjectsAndArrays) {
    EXPECT_EQ(detail::parse_body(R"({"a":1})")["a"].get<int>(), 1);
    EXPECT_EQ(detail::parse_body("[1,2,3]").size(), 3u);
}

TEST(ParseBody, MalformedJsonThrowsParseErrorCarryingTheBody) {
    try {
        detail::parse_body("{not json");
        FAIL() << "parse_body must throw";
    }
    catch (const parse_error &e) {
        EXPECT_EQ(e.body, "{not json");
    }
}

// ---------------------------------------------------------------------------
// Credentials
// ---------------------------------------------------------------------------

TEST(Credentials, TradingHeadersUseApcaKeyHeaders) {
    const credentials creds("my-key", "my-secret");
    const auto headers = creds.trading_headers();

    ASSERT_EQ(headers.size(), 2u);
    EXPECT_EQ(headers[0].first, "APCA-API-KEY-ID");
    EXPECT_EQ(headers[0].second, "my-key");
    EXPECT_EQ(headers[1].first, "APCA-API-SECRET-KEY");
    EXPECT_EQ(headers[1].second, "my-secret");
}

TEST(Credentials, BasicAuthHeaderIsBase64OfKeyColonSecret) {
    // Broker API authenticates with HTTP Basic.
    const credentials creds("key", "secret");
    const auto headers = creds.basic_auth_headers();

    ASSERT_EQ(headers.size(), 1u);
    EXPECT_EQ(headers[0].first, "Authorization");
    EXPECT_EQ(headers[0].second, "Basic a2V5OnNlY3JldA==");   // base64("key:secret")
}

TEST(Credentials, OauthTokenTakesPrecedenceOverKeyPair) {
    credentials creds("key", "secret");
    creds.oauth_token = "tok";

    EXPECT_TRUE(creds.uses_oauth());
    const auto headers = creds.trading_headers();
    ASSERT_EQ(headers.size(), 1u);
    EXPECT_EQ(headers[0].first, "Authorization");
    EXPECT_EQ(headers[0].second, "Bearer tok");

    // The Broker scheme resolves to the same bearer header.
    EXPECT_EQ(creds.basic_auth_headers()[0].second, "Bearer tok");
}

TEST(Credentials, EmptyDetection) {
    EXPECT_TRUE(credentials{}.empty());
    EXPECT_TRUE(credentials("key", "").empty());
    EXPECT_TRUE(credentials("", "secret").empty());
    EXPECT_FALSE(credentials("key", "secret").empty());
    EXPECT_FALSE(credentials::from_oauth_token("tok").empty());
}

TEST(Credentials, StreamAuthMessage) {
    const credentials creds("key", "secret");
    const auto j = json::parse(creds.stream_auth_message());
    EXPECT_EQ(j["action"], "auth");
    EXPECT_EQ(j["key"], "key");
    EXPECT_EQ(j["secret"], "secret");
}

TEST(Credentials, StreamAuthMessageUsesDocumentedOauthForm) {
    const auto creds = credentials::from_oauth_token("tok");
    const auto j = json::parse(creds.stream_auth_message());
    EXPECT_EQ(j["key"], "oauth");
    EXPECT_EQ(j["secret"], "tok");
}

TEST(Base64, KnownVectors) {
    EXPECT_EQ(base64_encode(""), "");
    EXPECT_EQ(base64_encode("f"), "Zg==");
    EXPECT_EQ(base64_encode("fo"), "Zm8=");
    EXPECT_EQ(base64_encode("foo"), "Zm9v");
    EXPECT_EQ(base64_encode("foobar"), "Zm9vYmFy");
}

}   // namespace alpaca::tests
