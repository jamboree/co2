/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_BLOCKING_HPP_INCLUDED
#define CO2_BLOCKING_HPP_INCLUDED

#include <mutex>
#include <thread>
#include <atomic>
#include <cstddef>
#include <system_error>
#include <type_traits>
#include <condition_variable>
#include <co2/coroutine.hpp>
#include <co2/detail/fixed_storage.hpp>

namespace co2 { namespace blocking_detail
{
    struct promise_base
    {
        bool initial_suspend() noexcept
        {
            return false;
        }

        bool cancellation_requested() const noexcept
        {
            return false;
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

        std::mutex mtx;
        std::condition_variable cond;
        int ready = false;
    };

    struct task
    {
        struct promise_type : promise_base
        {
            bool final_suspend() noexcept
            {
                cond.notify_one();
                return true;
            }

            task get_return_object(coroutine<promise_type>& coro)
            {
                task ret(this);
                coro.resume();
                return ret;
            }

            void wait()
            {
                std::unique_lock<std::mutex> lock(mtx);
                while (!ready)
                    cond.wait(lock);
                if (ready == -1)
                    throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
        };

        explicit task(promise_type* promise) : promise(promise) {}

        struct finalizer
        {
            promise_type* promise;

            ~finalizer()
            {
                coroutine<promise_type>::destroy(promise);
            }
        };

        template<class Awaitable, class Alloc>
        static auto run(Awaitable& a, Alloc alloc) CO2_BEG(task, (a) new(alloc), CO2_TEMP_SIZE(0);)
        {
            CO2_SUSPEND([&](co2::coroutine<>& c) { return co2::await_suspend(a, c); });
        } CO2_END
            
        using frame_storage = detail::fixed_storage<detail::frame_size<
            promise_type, void*, detail::fixed_allocator_base, 0>>;

        promise_type* promise;
    };

    struct timed_task
    {
        struct promise_type : promise_base
        {
            bool final_suspend() noexcept
            {
                cond.notify_one();
                return !last_owner.test_and_set(std::memory_order_relaxed);
            }

            timed_task get_return_object(coroutine<promise_type>& coro)
            {
                timed_task ret(this);
                coro.resume();
                return ret;
            }

            template<class Clock, class Duration>
            bool wait_until(std::chrono::time_point<Clock, Duration> const& timeout_time)
            {
                std::unique_lock<std::mutex> lock(mtx);
                while (!ready)
                {
                    if (cond.wait_until(lock, timeout_time) == std::cv_status::timeout)
                        break;
                }
                if (ready == -1)
                    throw std::system_error(std::make_error_code(std::errc::operation_canceled));
                return !!ready;
            }

            std::atomic_flag last_owner = ATOMIC_FLAG_INIT;
        };

        explicit timed_task(promise_type* promise) : promise(promise) {}

        struct finalizer
        {
            promise_type* promise;

            ~finalizer()
            {
                if (promise->last_owner.test_and_set(std::memory_order_relaxed))
                    coroutine<promise_type>::destroy(promise);
            }
        };

        template<class Awaitable>
        static auto run(Awaitable a) CO2_BEG(timed_task, (a), CO2_TEMP_SIZE(0);)
        {
            CO2_SUSPEND([&](co2::coroutine<>& c) { return co2::await_suspend(a, c); });
        } CO2_END

        promise_type* promise;
    };
}}

namespace co2
{
    template<class Awaitable>
    void wait(Awaitable&& a)
    {
        if (await_ready(a))
            return;

        blocking_detail::task::frame_storage mem;
        auto task(blocking_detail::task::run(a, mem.alloc()));
        blocking_detail::task::finalizer _{task.promise};
        task.promise->wait();
    }

    template<class Awaitable, class Clock, class Duration>
    bool wait_until(Awaitable&& a, std::chrono::time_point<Clock, Duration> const& timeout_time)
    {
        if (await_ready(a))
            return true;

        auto task(blocking_detail::timed_task::run<Awaitable>(std::forward<Awaitable>(a)));
        blocking_detail::timed_task::finalizer _{task.promise};
        return task.promise->wait_until(timeout_time);
    }

    template<class Awaitable, class Rep, class Period>
    inline bool wait_for(Awaitable&& a, std::chrono::duration<Rep, Period> const& rel_time)
    {
        return wait_until(std::forward<Awaitable>(a), std::chrono::steady_clock::now() + rel_time);
    }

    template<class Awaitable>
    inline decltype(auto) get(Awaitable&& a)
    {
        wait(a);
        return await_resume(a);
    }
}

#endif