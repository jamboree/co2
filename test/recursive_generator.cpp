/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <co2/recursive_generator.hpp>
#include "common.hpp"
#include "range_algo.hpp"

auto to(int i) CO2_BEG(co2::recursive_generator<int>, (i))
{
    if (i)
        CO2_YIELD(to(i - 1));
    CO2_YIELD(i);
} CO2_END

auto throws_depth(int i, int& terminated) CO2_BEG(co2::recursive_generator<int>, (i, terminated),
    inc_on_finalize _{terminated};
)
{
    if (i)
        CO2_YIELD(throws_depth(i - 1, terminated));
    else
        throw ball();
} CO2_END

auto forever_depth(int i, int& terminated) CO2_BEG(co2::recursive_generator<int>, (i, terminated),
    inc_on_finalize _{terminated};
)
{
    if (i)
        CO2_YIELD(forever_depth(i - 1, terminated));
    else
        for (;;)
            CO2_YIELD(0);
} CO2_END

TEST_CASE("move check")
{
    auto gen = to(10);
    auto moved = std::move(gen);
    CHECK(empty(gen));
    CHECK_FALSE(empty(moved));
}

TEST_CASE("value check")
{
    auto gen = to(10);
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
    int terminated = 0;
    auto gen = throws_depth(3, terminated);
    CHECK_THROWS_AS(gen.begin(), ball);
    CHECK(terminated == 4);
    CHECK(empty(gen));
}

TEST_CASE("unwind check")
{
    int terminated = 0;
    auto gen = forever_depth(4, terminated);
    CHECK_FALSE(terminated);
    gen.begin();
    CHECK_FALSE(terminated);
    gen = {};
    CHECK(terminated == 5);
}