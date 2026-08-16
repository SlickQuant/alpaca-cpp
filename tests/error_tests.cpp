// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline unit tests for error mapping and auth header construction.

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include <alpaca/auth.hpp>
#include <alpaca/trading_client.hpp>
#include <alpaca/utils.hpp>
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

TEST(Credentials, ResolveKeepsExplicitCredentialsUntouched) {
    // A caller who passes credentials must never have them silently replaced by whatever
    // happens to be in the environment.
    const credentials explicit_creds("explicit-key", "explicit-secret");

    for (auto env : {environment::paper, environment::live, environment::sandbox}) {
        const auto resolved = credentials::resolve(explicit_creds, env);
        EXPECT_EQ(resolved.api_key_id, "explicit-key");
        EXPECT_EQ(resolved.api_secret_key, "explicit-secret");
    }

    const auto oauth = credentials::resolve(credentials::from_oauth_token("tok"), environment::live);
    EXPECT_EQ(oauth.oauth_token, "tok");
}

TEST(Credentials, ResolveFillsEmptyCredentialsFromTheEnvironment) {
    // Empty in, environment out — this is what makes `trading_client client;` work.
    const auto resolved = credentials::resolve({}, environment::paper);
    EXPECT_EQ(resolved.api_key_id, credentials::from_env(environment::paper).api_key_id);
}

namespace {

/// Sets an environment variable for the duration of a scope and restores it afterwards, so
/// the precedence rules can be tested deterministically rather than against whatever the
/// developer's machine happens to export.
class scoped_env {
public:
    scoped_env(const char *name, const char *value)
        : name_(name)
        , had_previous_(!get_env(name).empty())
        , previous_(get_env(name)) {
        set(name_.c_str(), value);
    }

    ~scoped_env() {
        set(name_.c_str(), had_previous_ ? previous_.c_str() : nullptr);
    }

    scoped_env(const scoped_env &) = delete;
    scoped_env& operator=(const scoped_env &) = delete;

private:
    static void set(const char *name, const char *value) {
#ifdef _MSC_VER
        // An empty value removes the variable on Windows.
        _putenv_s(name, value ? value : "");
#else
        if (value) {
            setenv(name, value, 1);
        }
        else {
            unsetenv(name);
        }
#endif
    }

    std::string name_;
    bool had_previous_;
    std::string previous_;
};

}   // namespace

TEST(Credentials, PaperPrefersThePaperVariables) {
    const scoped_env live_key("APCA_API_KEY_ID", "AKLIVE");
    const scoped_env live_secret("APCA_API_SECRET_KEY", "live-secret");
    const scoped_env paper_key("APCA_PAPER_API_KEY_ID", "PKPAPER");
    const scoped_env paper_secret("APCA_PAPER_API_SECRET_KEY", "paper-secret");

    const auto paper = credentials::from_env(environment::paper);
    EXPECT_EQ(paper.api_key_id, "PKPAPER");
    EXPECT_EQ(paper.api_secret_key, "paper-secret");
}

TEST(Credentials, LiveAndSandboxAlwaysReadTheUnprefixedVariables) {
    // Only paper has its own pair; a paper key must never leak into a live client.
    const scoped_env live_key("APCA_API_KEY_ID", "AKLIVE");
    const scoped_env live_secret("APCA_API_SECRET_KEY", "live-secret");
    const scoped_env paper_key("APCA_PAPER_API_KEY_ID", "PKPAPER");
    const scoped_env paper_secret("APCA_PAPER_API_SECRET_KEY", "paper-secret");

    for (auto env : {environment::live, environment::sandbox}) {
        const auto resolved = credentials::from_env(env);
        EXPECT_EQ(resolved.api_key_id, "AKLIVE");
        EXPECT_EQ(resolved.api_secret_key, "live-secret");
    }

    // The no-argument overload keeps its original meaning.
    EXPECT_EQ(credentials::from_env().api_key_id, "AKLIVE");
}

TEST(Credentials, PaperFallsBackToUnprefixedWhenPaperVariablesAreUnset) {
    // A single-account setup that only exports APCA_API_* must keep working.
    const scoped_env live_key("APCA_API_KEY_ID", "AKONLY");
    const scoped_env live_secret("APCA_API_SECRET_KEY", "only-secret");
    const scoped_env paper_key("APCA_PAPER_API_KEY_ID", nullptr);
    const scoped_env paper_secret("APCA_PAPER_API_SECRET_KEY", nullptr);

    const auto paper = credentials::from_env(environment::paper);
    EXPECT_EQ(paper.api_key_id, "AKONLY");
    EXPECT_EQ(paper.api_secret_key, "only-secret");
}

TEST(Credentials, HalfConfiguredPaperPairFallsBackRatherThanSendingAnEmptySecret) {
    // Only the key set, no secret: the pair is unusable, so falling back beats sending a
    // request with an empty secret that would fail with a confusing 401.
    const scoped_env live_key("APCA_API_KEY_ID", "AKONLY");
    const scoped_env live_secret("APCA_API_SECRET_KEY", "only-secret");
    const scoped_env paper_key("APCA_PAPER_API_KEY_ID", "PKPAPER");
    const scoped_env paper_secret("APCA_PAPER_API_SECRET_KEY", nullptr);

    const auto paper = credentials::from_env(environment::paper);
    EXPECT_EQ(paper.api_key_id, "AKONLY");
    EXPECT_EQ(paper.api_secret_key, "only-secret");
}

TEST(Credentials, SurroundingWhitespaceIsStrippedFromEnvironmentValues) {
    // Regression: a secret exported with a trailing newline authenticated fine over REST,
    // because the value goes into an HTTP header and the stray byte is trimmed in
    // transit, but was rejected by the websocket streams with a bare 401 — there the
    // secret is embedded in a JSON string and compared exactly. The split behaviour made
    // it look like a bug in the stream client rather than in the environment.
    const scoped_env key("APCA_API_KEY_ID", "AKTRAILING\n");
    const scoped_env secret("APCA_API_SECRET_KEY", "  secret-with-space  ");

    const auto creds = credentials::from_env();
    EXPECT_EQ(creds.api_key_id, "AKTRAILING");
    EXPECT_EQ(creds.api_secret_key, "secret-with-space");

    // The auth frame the streams send must carry the cleaned value, since that is where
    // the original failure actually surfaced.
    const auto message = json::parse(creds.stream_auth_message());
    EXPECT_EQ(message["key"], "AKTRAILING");
    EXPECT_EQ(message["secret"], "secret-with-space");
}

TEST(Credentials, WhitespaceOnlyValueIsTreatedAsUnset) {
    const scoped_env key("APCA_API_KEY_ID", "   ");
    const scoped_env secret("APCA_API_SECRET_KEY", "\n");
    // Cleared explicitly: an OAuth token in the developer's real environment would
    // otherwise make the credentials non-empty and mask what this is checking.
    const scoped_env token("APCA_API_OAUTH_TOKEN", nullptr);

    EXPECT_TRUE(credentials::from_env().empty());
}

TEST(Credentials, InteriorWhitespaceIsPreserved) {
    // Only the ends are trimmed; a value that legitimately contains a space must survive.
    const scoped_env token("APCA_API_OAUTH_TOKEN", " a b \n");

    EXPECT_EQ(credentials::from_env().oauth_token, "a b");
}

TEST(Credentials, ClientsResolveTheirEnvironmentsCredentials) {
    const scoped_env live_key("APCA_API_KEY_ID", "AKLIVE");
    const scoped_env live_secret("APCA_API_SECRET_KEY", "live-secret");
    const scoped_env paper_key("APCA_PAPER_API_KEY_ID", "PKPAPER");
    const scoped_env paper_secret("APCA_PAPER_API_SECRET_KEY", "paper-secret");

    // Constructing a client must pick up the pair matching its environment, which is the
    // whole point of resolving lazily rather than in a default argument.
    const trading_client paper;
    EXPECT_EQ(paper.context().creds().api_key_id, "PKPAPER");

    const trading_client live({}, environment::live);
    EXPECT_EQ(live.context().creds().api_key_id, "AKLIVE");

    // An explicit argument still wins over both.
    const trading_client explicit_creds(credentials("EXPLICIT", "s"), environment::paper);
    EXPECT_EQ(explicit_creds.context().creds().api_key_id, "EXPLICIT");
}

TEST(Base64, KnownVectors) {
    EXPECT_EQ(base64_encode(""), "");
    EXPECT_EQ(base64_encode("f"), "Zg==");
    EXPECT_EQ(base64_encode("fo"), "Zm8=");
    EXPECT_EQ(base64_encode("foo"), "Zm9v");
    EXPECT_EQ(base64_encode("foobar"), "Zm9vYmFy");
}

}   // namespace alpaca::tests
