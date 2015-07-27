#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <co2/coroutine.hpp>

auto times(int i) CO2_BEG(co2::coroutine<>, (i))
{
    while (i--)
        CO2_AWAIT(co2::suspend_always{});
} CO2_END

struct ball {};

auto throws_nth(int i) CO2_BEG(co2::coroutine<>, (i))
{
    while (i--)
        CO2_AWAIT(co2::suspend_always{});
    throw ball();
} CO2_END

auto forever(bool& terminated) CO2_BEG(co2::coroutine<>, (terminated),
    struct f
    {
        bool& terminated;
        ~f() { terminated = true; }
    } on_unwind_set{terminated};
)
{
    for (;;)
        CO2_AWAIT(co2::suspend_always{});
} CO2_END

int total_times(co2::coroutine<>& coro)
{
    int i = 0;
    while (coro)
    {
        coro();
        ++i;
    }
    return i;
}

TEST_CASE("move check")
{
    auto coro = times(5);
    auto moved = std::move(coro);
    CHECK_FALSE(coro);
    CHECK(moved);
}

TEST_CASE("times check")
{
    SECTION("5 times")
    {
        auto coro = times(5);
        CHECK(total_times(coro) == 5);
    }
    SECTION("0 times")
    {
        auto coro = times(0);
        CHECK_FALSE(coro);
    }
}

TEST_CASE("throw check")
{
    CHECK_THROWS_AS(throws_nth(0), ball);
    SECTION("throw at 2nd call")
    {
        auto coro = throws_nth(2);
        CHECK_NOTHROW(coro.resume());
        CHECK_THROWS_AS(coro.resume(), ball);
        CHECK_FALSE(coro);
    }
}

TEST_CASE("unwind check")
{
    bool terminated = false;
    auto coro = forever(terminated);
    CHECK_FALSE(terminated);
    coro();
    CHECK_FALSE(terminated);
    coro.reset();
    CHECK(terminated);
}