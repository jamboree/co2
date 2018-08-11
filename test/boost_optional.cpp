/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2018 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <co2/adapted/boost_optional.hpp>
#include <boost/optional/optional_io.hpp>
#include "common.hpp"

auto plus(boost::optional<int> i, int n) CO2_BEG(boost::optional<int>, (i, n))
{
    CO2_AWAIT_APPLY(n +=, i);
    CO2_RETURN(n);
} CO2_END

auto hang(int& terminated) CO2_BEG(boost::optional<int>, (terminated),
    inc_on_finalize _{terminated};
)
{
    CO2_AWAIT(co2::suspend_always{});
    CO2_RETURN(42);
} CO2_END

TEST_CASE("normal check")
{
    CHECK_FALSE(plus({}, 5));
    {
        auto ret(plus(6, 5));
        REQUIRE(ret);
        CHECK(ret.get() == 11);
    }
}

TEST_CASE("abort check")
{
    int terminated = 0;
    CHECK_FALSE(hang(terminated));
    CHECK(terminated);
}