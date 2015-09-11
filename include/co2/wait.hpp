/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_WAIT_HPP_INCLUDED
#define CO2_WAIT_HPP_INCLUDED

#include <mutex>
#include <thread>
#include <cstddef>
#include <system_error>
#include <type_traits>
#include <condition_variable>
#include <co2/coroutine.hpp>

namespace co2 { namespace wait_detail
{
    template<class Task>
    struct wrapper
    {
        Task& task;

        bool await_ready() noexcept
        {
            return false;
        }

        template<class F>
        auto await_suspend(F&& f) -> decltype(co2::await_suspend(task, std::forward<F>(f)))
        {
            return co2::await_suspend(task, std::forward<F>(f));
        }

        void await_resume() noexcept {}
    };

    struct task
    {
        struct promise_type
        {
            bool initial_suspend() noexcept
            {
                return false;
            }

            bool final_suspend() noexcept
            {
                cond.notify_one();
                return true;
            }

            bool cancellation_requested() const noexcept
            {
                return false;
            }

            task get_return_object(coroutine<promise_type>& coro)
            {
                task ret(this);
                coro.resume();
                return ret;
            }

            void set_result() noexcept
            {
                std::unique_lock<std::mutex> lock(mtx);
                ready = true;
            }

            void cancel() noexcept
            {
                std::unique_lock<std::mutex> lock(mtx);
                ready = -1;
            }

            void wait()
            {
                std::unique_lock<std::mutex> lock(mtx);
                while (!ready)
                    cond.wait(lock);
                if (ready == -1)
                    throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }

            std::mutex mtx;
            std::condition_variable cond;
            int ready = false;
        };

        explicit task(promise_type* promise) : promise(promise) {}

        task(task&& other) : promise(other.promise)
        {
            other.promise = nullptr;
        }

        ~task()
        {
            if (promise)
                coroutine<promise_type>::destroy(promise);
        }

        promise_type* promise;
    };

    template<class Awaitable>
    auto run(Awaitable& a) CO2_BEG(task, (a), CO2_TEMP_SIZE(sizeof(void*));)
    {
        CO2_AWAIT(wrapper<Awaitable>{a});
    } CO2_END
}}

namespace co2
{
    template<class Awaitable>
    void wait(Awaitable&& a)
    {
        if (await_ready(a))
            return;

        auto task(wait_detail::run(a));
        task.promise->wait();
    }

    template<class Awaitable>
    inline decltype(auto) get(Awaitable&& a)
    {
        wait(a);
        return await_resume(a);
    }
}

#endif