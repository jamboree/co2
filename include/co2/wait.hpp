/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_WAIT_HPP_INCLUDED
#define CO2_WAIT_HPP_INCLUDED

#include <mutex>
#include <thread>
#include <condition_variable>
#include <co2/coroutine.hpp>

namespace co2 { namespace wait_detail
{
    struct result
    {
        struct promise_type
        {
            suspend_never initial_suspend()
            {
                return{};
            }

            void finalize() noexcept {}

            bool cancellation_requested() const
            {
                return false;
            }

            result get_return_object()
            {
                return {};
            }

            void set_result() {}
        };
    };

    template<class Awaitable>
    auto run(Awaitable& a, std::mutex& mtx, std::condition_variable& cond, bool& not_ready)
    CO2_RET(result, (a, mtx, cond, not_ready))
    {
        CO2_AWAIT(awaken(a));
        {
            std::unique_lock<std::mutex> lock(mtx);
            not_ready = false;
        }
        cond.notify_one();
    } CO2_END
}}

namespace co2
{
    template<class Awaitable>
    void wait(Awaitable&& a)
    {
        if (await_ready(a))
            return;

        std::mutex mtx;
        std::condition_variable cond;
        bool not_ready = true;
        wait_detail::run(a, mtx, cond, not_ready);
        std::unique_lock<std::mutex> lock(mtx);
        while (not_ready) cond.wait(lock);
    }

    template<class Awaitable>
    inline decltype(auto) get(Awaitable&& a)
    {
        wait(a);
        return await_resume(a);
    }
}

#endif