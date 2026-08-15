// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Convenience umbrella header. Including individual headers keeps compile times lower;
// this one exists for quick starts and examples.

#pragma once

#include <alpaca/auth.hpp>
#include <alpaca/common.hpp>
#include <alpaca/environment.hpp>
#include <alpaca/error.hpp>
#include <alpaca/rate_limiter.hpp>
#include <alpaca/utils.hpp>

#include <alpaca/trading/account.hpp>
#include <alpaca/trading/activity.hpp>
#include <alpaca/trading/asset.hpp>
#include <alpaca/trading/calendar.hpp>
#include <alpaca/trading/crypto_funding.hpp>
#include <alpaca/trading/locate.hpp>
#include <alpaca/trading/option_contract.hpp>
#include <alpaca/trading/order.hpp>
#include <alpaca/trading/position.hpp>
#include <alpaca/trading/watchlist.hpp>

#include <alpaca/trading_client.hpp>
#include <alpaca/trading_client_awaitable.hpp>
