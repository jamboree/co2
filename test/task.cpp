/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <co2/task.hpp>
#include "common.hpp"
#include "trigger.hpp"

auto wait(trigger<int>& t) CO2_BEG(co2::task<int>, (t))
{
    CO2_AWAIT_RETURN(t);
} CO2_END

auto throws(trigger<int>& t) CO2_BEG(co2::task<>, (t))
{
    CO2_AWAIT(t);
    throw ball();
} CO2_END

auto suspend(int& terminated) CO2_BEG(co2::task<>, (terminated),
    inc_on_finalize _{terminated};
)
{
    CO2_AWAIT(co2::suspend_always{});
} CO2_END

auto wait(trigger<int>& t, int& terminated) CO2_BEG(co2::task<>, (t, terminated),
    inc_on_finalize _{terminated};
)
{
    CO2_AWAIT(t);
} CO2_END

auto follow(co2::task<> task, int& terminated) CO2_BEG(co2::task<>, (task, terminated),
    inc_on_finalize _{terminated};
)
{
    CO2_AWAIT(task);
} CO2_END

auto inc(co2::task<int> prev) CO2_BEG(co2::task<int>, (prev),
    int n;
)
{
    CO2_AWAIT_SET(n, prev);
    CO2_RETURN(n + 1);
} CO2_END

TEST_CASE("move check")
{
    trigger<int> event;
    auto task = wait(event);
    auto moved = std::move(task);
    CHECK_FALSE(task);
    CHECK(moved);
}

TEST_CASE("value check")
{
    trigger<int> event;
    auto task = wait(event);
    CHECK_FALSE(task.await_ready());
    event(16);
    REQUIRE(task.await_ready());
    CHECK(task.await_resume() == 16);
    CHECK_FALSE(task);
}

TEST_CASE("throw check")
{
    trigger<int> event;
    auto task = throws(event);
    CHECK_FALSE(task.await_ready());
    event(16);
    REQUIRE(task.await_ready());
    CHECK_THROWS_AS(task.await_resume(), ball);
    CHECK_FALSE(task);
}

TEST_CASE("unwind check")
{
    int terminated = 0;
    trigger<int> event;
    auto task = wait(event, terminated);
    CHECK_FALSE(task.await_ready());
    CHECK_FALSE(terminated);
    task.reset();
    CHECK_FALSE(terminated);
    event(16);
    CHECK(terminated);
}

TEST_CASE("abort check")
{
    int terminated = 0;
    auto task = suspend(terminated);
    CHECK(terminated);
    CHECK_THROWS_AS(task.await_resume(), co2::task_cancelled);
}

TEST_CASE("cancel check")
{
    int terminated = 0;
    trigger<int> event;
    auto task = follow(wait(event, terminated), terminated);
    event.cancel();
    REQUIRE(task);
    REQUIRE(task.await_ready());
    CHECK_THROWS_AS(task.await_resume(), co2::task_cancelled);
    CHECK(terminated == 2);
}

TEST_CASE("recursion check")
{
    trigger<int> event;
    auto t = wait(event);
    for (int i = 0; i != 65536; ++i)
        t = inc(std::move(t));
    event(0);
    REQUIRE(t);
    REQUIRE(t.await_ready());
    CHECK(t.await_resume() == 65536);
}

TEST_CASE("recursion cancel check")
{
    int terminated = 0;
    trigger<int> event;
    auto t = wait(event, terminated);
    for (int i = 0; i != 65536; ++i)
        t = follow(std::move(t), terminated);
    event.cancel();
    REQUIRE(t);
    REQUIRE(t.await_ready());
    CHECK_THROWS_AS(t.await_resume(), co2::task_cancelled);
    CHECK(terminated == 65537);
}