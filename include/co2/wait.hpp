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

namespace co2 { namespace detail
{
    template<class Awaitable>
    auto wait_impl(Awaitable& a, std::mutex& mtx, std::condition_variable& cond, bool& done)
    CO2_RET(co2::coroutine<>, (a, mtx, cond, done))
    {
        CO2_AWAIT(co2::awaken(a));
        {
            std::unique_lock<std::mutex> lock(mtx);
            done = true;
        }
        cond.notify_one();
    } CO2_END
}}

namespace co2
{
    template<class Awaitable>
    void wait(Awaitable&& a)
    {
        if (co2::await_ready(a))
            return;

        std::mutex mtx;
        std::condition_variable cond;
        std::unique_lock<std::mutex> lock(mtx);
        bool done = false;
        detail::wait_impl(a, mtx, cond, done);
        cond.wait(lock, [&done] { return done; });
    }

    template<class Awaitable>
    inline decltype(auto) get(Awaitable&& a)
    {
        wait(a);
        return co2::await_resume(a);
    }
}

#endif