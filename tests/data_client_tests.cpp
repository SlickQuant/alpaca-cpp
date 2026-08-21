// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline tests for Market Data client construction. Nothing here touches the network:
// non-empty credentials short-circuit `credentials::resolve`, so the constructors only
// pick a base URL and record the environment.

#include <gtest/gtest.h>

#include <alpaca/data_client.hpp>
#include <alpaca/data_client_awaitable.hpp>
#include <alpaca/environment.hpp>

namespace alpaca::tests {

namespace {
credentials test_creds() { return credentials("PKTEST", "secret"); }
}   // namespace

// `data.alpaca.markets` is not account-scoped, so the environment shows up only in the
// key pair the client resolved and — for sandbox — in the base URL. Keeping env()
// readable is what makes the stored environment observable at all.
TEST(DataClient, RecordsTheEnvironmentItWasBuiltFor) {
    const data_client paper(test_creds(), environment::paper);
    EXPECT_EQ(paper.env(), environment::paper);
    EXPECT_EQ(paper.base_url(), urls::market_data);

    const data_client live(test_creds(), environment::live);
    EXPECT_EQ(live.env(), environment::live);
    EXPECT_EQ(live.base_url(), urls::market_data);

    const data_client sandbox(test_creds(), environment::sandbox);
    EXPECT_EQ(sandbox.env(), environment::sandbox);
    EXPECT_EQ(sandbox.base_url(), urls::sandbox_data);
}

// The explicit-base-URL constructor has no environment argument; it resolves paper
// credentials, so that is what env() must report.
TEST(DataClient, ExplicitBaseUrlReportsPaper) {
    const data_client client(test_creds(), std::string("https://proxy.example.com"));
    EXPECT_EQ(client.env(), environment::paper);
    EXPECT_EQ(client.base_url(), "https://proxy.example.com");
}

TEST(DataClientAwaitable, RecordsTheEnvironmentItWasBuiltFor) {
    const data_client_awaitable paper(test_creds(), environment::paper);
    EXPECT_EQ(paper.env(), environment::paper);
    EXPECT_EQ(paper.base_url(), urls::market_data);

    const data_client_awaitable sandbox(test_creds(), environment::sandbox);
    EXPECT_EQ(sandbox.env(), environment::sandbox);
    EXPECT_EQ(sandbox.base_url(), urls::sandbox_data);
}

TEST(DataClientAwaitable, ExplicitBaseUrlReportsPaper) {
    const data_client_awaitable client(test_creds(), std::string("https://proxy.example.com"));
    EXPECT_EQ(client.env(), environment::paper);
    EXPECT_EQ(client.base_url(), "https://proxy.example.com");
}

}   // namespace alpaca::tests
