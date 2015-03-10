#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <co2/coroutine.hpp>

struct set_on_destruction
{
    bool& flag;

    set_on_destruction(bool& flag) : flag(flag) {}

    ~set_on_destruction()
    {
        flag = true;
    }
};

struct promise
{
    co2::suspend_always initial_suspend()
    {
        return{};
    }

    co2::suspend_never final_suspend()
    {
        return{};
    }

    bool cancellation_requested() const
    {
        return false;
    }

    co2::coroutine<promise> get_return_object()
    {
        return co2::coroutine<promise>(this);
    }

    void set_result() {}

    void set_exception(std::exception_ptr const&) {}

    void set_on_dtor(bool& flag)
    {
        pflag = &flag;
    }

    ~promise()
    {
        *pflag = true;
    }

    bool* pflag;
};

struct thrown {};

struct thrower
{
    thrower()
    {
        throw thrown();
    }
};

auto finished_work(bool& finished, bool& exited) CO2_RET(co2::coroutine<>, (finished, exited),
    set_on_destruction _{exited};
)
{
    finished = true;
} CO2_END

auto unfinished_work_with_promise(bool& finished, bool& exited) CO2_RET(co2::coroutine<promise>, (finished, exited),
    set_on_destruction _{exited};
)
{
    finished = true;
} CO2_END

auto throw_at_init(bool& exited) CO2_RET(co2::coroutine<>, (exited),
    set_on_destruction _{exited};
    thrower t;
)
{
    /*none*/
} CO2_END

SCENARIO("coroutine management")
{
    GIVEN("a null coroutine")
    {
        bool finished = false;
        bool exited = false;
        co2::coroutine<> coro;
        CHECK(!coro);
        WHEN("assigned a finished work")
        {
            coro = finished_work(finished, exited);
            CHECK(finished);
            THEN("it's valid and exited")
            {
                REQUIRE(coro);
                CHECK(coro.done());
                CHECK(exited);
                AND_WHEN("reset")
                {
                    coro.reset();
                    THEN("it's invalid")
                        CHECK(!coro);
                }
                AND_WHEN("copied")
                {
                    co2::coroutine<> copy(coro);
                    THEN("both are valid and equal")
                    {
                        REQUIRE(coro);
                        REQUIRE(copy);
                        CHECK(copy == coro);
                    }
                }
                AND_WHEN("moved")
                {
                    co2::coroutine<> move(std::move(coro));
                    THEN("only the new one is valid")
                    {
                        CHECK(!coro);
                        CHECK(move);
                        CHECK(move.done());
                    }
                }
                AND_WHEN("detached")
                {
                    auto handle = coro.detach();
                    THEN("it's invalid and the handle is valid")
                    {
                        CHECK(!coro);
                        CHECK(handle);
                        AND_WHEN("constructed from the handle")
                        {
                            co2::coroutine<> coro2(handle);
                            THEN("the new one is valid")
                                CHECK(coro2);
                        }
                    }
                }
            }
        }
        WHEN("assigned a null handle")
        {
            coro = co2::coroutine<>(nullptr);
            THEN("it's still invalid")
                CHECK(!coro);
        }
        WHEN("copied")
        {
            co2::coroutine<> copy(coro);
            THEN("both are invalid and equal")
            {
                REQUIRE(!coro);
                REQUIRE(!copy);
                CHECK(copy == coro);
            }
        }
        WHEN("moved")
        {
            co2::coroutine<> move(std::move(coro));
            THEN("both are invalid and equal")
            {
                REQUIRE(!coro);
                REQUIRE(!move);
                CHECK(move == coro);
            }
        }
        WHEN("assigned a throwing init")
        {
            CHECK_THROWS_AS(coro = throw_at_init(exited), thrown);
            CHECK(exited);
            THEN("it's still invalid")
                CHECK(!coro);
        }
    }
    GIVEN("an unfinished work with promise")
    {
        bool finished = false;
        bool exited = false;
        bool destroyed = false;
        auto coro = unfinished_work_with_promise(finished, exited);
        REQUIRE(coro);
        REQUIRE(!coro.done());
        CHECK(!finished);
        CHECK(!exited);
        auto& p = coro.promise();
        p.set_on_dtor(destroyed);
        CHECK(!destroyed);
        WHEN("run")
        {
            coro();
            CHECK(finished);
            THEN("it's exited but not destroyed")
            {
                CHECK(coro.done());
                CHECK(exited);
                CHECK(!destroyed);
            }
            AND_WHEN("reset")
            {
                coro.reset();
                THEN("it's invalid and destroyed")
                {
                    CHECK(!coro);
                    CHECK(destroyed);
                }
            }
        }
        WHEN("constructed from the promise")
        {
            co2::coroutine<promise> coro2(&p);
            THEN("both are valid and equal")
            {
                CHECK(coro);
                CHECK(coro2);
                CHECK(coro == coro2);
            }
        }
        WHEN("reset")
        {
            coro.reset();
            THEN("it's invalid, exited and destroyed but not finsished")
            {
                CHECK(!coro);
                CHECK(exited);
                CHECK(destroyed);
                CHECK(!finished);
            }
        }
    }
}
