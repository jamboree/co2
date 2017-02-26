/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <co2/blocking.hpp>
#include "common.hpp"

struct pitcher
{
    bool await_ready() noexcept
    {
        return false;
    }

    void await_suspend(co2::coroutine<>&)
    {
        throw ball();
    }

    void await_resume() noexcept {}
};

TEST_CASE("throw check")
{
    CHECK_THROWS_AS(co2::wait(pitcher{}), ball);
    CHECK_THROWS_AS(co2::wait(co2::suspend_always{}), std::system_error);
}