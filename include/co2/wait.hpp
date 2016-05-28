/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2016 Jamboree

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
#include <co2/detail/fixed_storage.hpp>

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

    template<class Awaitable, class Alloc>
    auto run(Awaitable& a, Alloc alloc) CO2_BEG(task, (a) new(alloc), CO2_TEMP_SIZE(0);)
    {
        CO2_YIELD_WITH([&](co2::coroutine<>& c) { return co2::await_suspend(a, c); });
    } CO2_END

    using frame_storage = detail::fixed_storage<detail::frame_size<
        task::promise_type, void*[2], detail::fixed_allocator_base, 0>>;
}}

namespace co2
{
    template<class Awaitable>
    void wait(Awaitable&& a)
    {
        if (await_ready(a))
            return;

        using storage = wait_detail::frame_storage;
        storage mem;
        auto task(wait_detail::run(a, storage::allocator<>(mem)));
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