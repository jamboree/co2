#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <co2/generator.hpp>

auto range(int i, int e) CO2_BEG(co2::generator<int>, (i, e))
{
    for (; i != e; ++i)
        CO2_YIELD(i);
} CO2_END

struct ball {};

auto throws() CO2_BEG(co2::generator<int>, ())
{
    throw ball();
} CO2_END

auto forever(bool& terminated) CO2_BEG(co2::generator<int>, (terminated),
    int i = 0;
    struct f
    {
        bool& terminated;
        ~f() { terminated = true; }
    } on_unwind_set{terminated};
)
{
    for (;;)
        CO2_YIELD(i);
} CO2_END

int sum(co2::generator<int>& gen)
{
    int n = 0;
    for (auto i : gen)
        n += i;
    return n;
}

void skip(co2::generator<int>& gen, int n)
{
    if (!n)
        return;

    for (auto i : gen)
        if (!--n)
            break;
}

bool empty(co2::generator<int>& gen)
{
    return gen.begin() == gen.end();
}

TEST_CASE("move check")
{
    auto gen = range(1, 11);
    auto moved = std::move(gen);
    CHECK(empty(gen));
    CHECK_FALSE(empty(moved));
}

TEST_CASE("sum check")
{
    auto gen = range(1, 11);
    SECTION("all")
    {
        CHECK(sum(gen) == 55);
    }
    SECTION("skip 5")
    {
        skip(gen, 5);
        CHECK(sum(gen) == 40);
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
    bool terminated = false;
    auto gen = forever(terminated);
    CHECK_FALSE(terminated);
    gen.begin();
    CHECK_FALSE(terminated);
    gen = {};
    CHECK(terminated);
}