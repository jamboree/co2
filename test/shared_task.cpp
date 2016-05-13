/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <co2/shared_task.hpp>
#include "common.hpp"
#include "trigger.hpp"

auto wait(trigger<int>& t) CO2_BEG(co2::shared_task<int>, (t))
{
    CO2_AWAIT_RETURN(t);
} CO2_END

auto throws(trigger<int>& t) CO2_BEG(co2::shared_task<>, (t))
{
    CO2_AWAIT(t);
    throw ball();
} CO2_END

auto suspend(int& terminated) CO2_BEG(co2::shared_task<>, (terminated),
    inc_on_finalize _{terminated};
)
{
    CO2_AWAIT(co2::suspend_always{});
} CO2_END

auto wait(trigger<int>& t, int& terminated) CO2_BEG(co2::shared_task<>, (t, terminated),
    inc_on_finalize _{terminated};
)
{
    CO2_AWAIT(t);
} CO2_END

auto follow(co2::shared_task<> task, int& terminated) CO2_BEG(co2::shared_task<>, (task, terminated),
    inc_on_finalize _{terminated};
)
{
    CO2_AWAIT(task);
} CO2_END

auto x2(co2::shared_task<int> task) CO2_BEG(co2::shared_task<int>, (task), int i;)
{
    CO2_AWAIT_SET(i, task);
    CO2_RETURN(i * 2);
} CO2_END

auto inc(co2::shared_task<int> prev) CO2_BEG(co2::shared_task<int>, (prev),
    int n;
)
{
    CO2_AWAIT_SET(n, prev);
    CO2_RETURN(n + 1);
} CO2_END

auto count(co2::shared_task<int> task, int& n) CO2_BEG(co2::shared_task<>, (task, n))
{
    CO2_AWAIT(task);
    ++n;
} CO2_END

TEST_CASE("move check")
{
    trigger<int> event;
    auto task = wait(event);
    auto moved = std::move(task);
    CHECK_FALSE(task);
    CHECK(moved);
}

TEST_CASE("copy check")
{
    trigger<int> event;
    auto task = wait(event);
    auto copied = task;
    CHECK(task);
    CHECK(copied);
}

TEST_CASE("value check")
{
    trigger<int> event;
    auto task = wait(event);
    auto child1 = x2(task);
    auto child2 = x2(task);
    CHECK_FALSE(task.await_ready());
    CHECK_FALSE(child1.await_ready());
    CHECK_FALSE(child2.await_ready());
    event(16);
    REQUIRE(task.await_ready());
    REQUIRE(child1.await_ready());
    REQUIRE(child2.await_ready());
    CHECK(task.await_resume() == 16);
    CHECK(child1.await_resume() == 32);
    CHECK(child2.await_resume() == 32);
    CHECK(task);
    CHECK(child1);
    CHECK(child2);
}

TEST_CASE("throw check")
{
    trigger<int> event;
    auto task = throws(event);
    CHECK_FALSE(task.await_ready());
    event(16);
    REQUIRE(task.await_ready());
    CHECK_THROWS_AS(task.await_resume(), ball);
    CHECK(task);
}

TEST_CASE("unwind check")
{
    int terminated = 0;
    trigger<int> event;
    auto task = wait(event, terminated);
    follow(task, terminated);
    follow(task, terminated);
    CHECK_FALSE(task.await_ready());
    CHECK_FALSE(terminated);
    task.reset();
    CHECK_FALSE(terminated);
    event(16);
    CHECK(terminated == 3);
}

TEST_CASE("abort check")
{
    int terminated = 0;
    auto task = suspend(terminated);
    auto child1 = follow(task, terminated);
    auto child2 = follow(task, terminated);
    CHECK(terminated == 3);
    CHECK_THROWS_AS(task.await_resume(), co2::task_cancelled);
    CHECK_THROWS_AS(child1.await_resume(), co2::task_cancelled);
    CHECK_THROWS_AS(child2.await_resume(), co2::task_cancelled);
}

TEST_CASE("cancel check")
{
    int terminated = 0;
    trigger<int> event;
    auto task = wait(event, terminated);
    auto child1 = follow(task, terminated);
    auto child2 = follow(task, terminated);
    event.cancel();
    REQUIRE(task);
    REQUIRE(child1);
    REQUIRE(child2);
    REQUIRE(task.await_ready());
    REQUIRE(child1.await_ready());
    REQUIRE(child2.await_ready());
    CHECK_THROWS_AS(task.await_resume(), co2::task_cancelled);
    CHECK_THROWS_AS(child1.await_resume(), co2::task_cancelled);
    CHECK_THROWS_AS(child2.await_resume(), co2::task_cancelled);
    CHECK(terminated == 3);
}

TEST_CASE("recursion check")
{
    int n = 0;
    trigger<int> event;
    auto t = wait(event);
    for (int i = 0; i != 65536; ++i)
    {
        count(t, n);
        t = inc(t);
    }
    event(0);
    REQUIRE(t);
    REQUIRE(t.await_ready());
    CHECK(t.await_resume() == 65536);
    CHECK(n == 65536);
}

TEST_CASE("recursion cancel check")
{
    int terminated = 0;
    trigger<int> event;
    auto t = wait(event, terminated);
    for (int i = 0; i != 65536; ++i)
        t = follow(t, terminated);
    event.cancel();
    REQUIRE(t);
    REQUIRE(t.await_ready());
    CHECK_THROWS_AS(t.await_resume(), co2::task_cancelled);
    CHECK(terminated == 65537);
}