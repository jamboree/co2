/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <co2/generator.hpp>
#include "common.hpp"
#include "range_algo.hpp"

auto range(int i, int e) CO2_BEG(co2::generator<int>, (i, e))
{
    for (; i != e; ++i)
        CO2_YIELD(i);
} CO2_END

auto throws() CO2_BEG(co2::generator<int>, ())
{
    throw ball();
} CO2_END

auto forever(int& terminated) CO2_BEG(co2::generator<int>, (terminated),
    inc_on_finalize _{terminated};
)
{
    for (;;)
        CO2_YIELD(0);
} CO2_END

TEST_CASE("move check")
{
    auto gen = range(1, 11);
    auto moved = std::move(gen);
    CHECK(empty(gen));
    CHECK_FALSE(empty(moved));
}

TEST_CASE("value check")
{
    auto gen = range(0, 10);
    SECTION("all")
    {
        CHECK(equal_since(gen, 0));
    }
    SECTION("skip 5")
    {
        skip(gen, 5);
        CHECK(equal_since(gen, 5));
    }
}

TEST_CASE("throw check")
{
    auto gen = throws();
    CHECK_THROWS_AS(gen.begin(), ball);
    CHECK(empty(gen));
}

TEST_CASE("unwind check")
{
    int terminated = 0;
    auto gen = forever(terminated);
    CHECK_FALSE(terminated);
    gen.begin();
    CHECK_FALSE(terminated);
    gen = {};
    CHECK(terminated);
}