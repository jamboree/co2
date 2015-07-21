/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_TASK_HPP_INCLUDED
#define CO2_TASK_HPP_INCLUDED

#include <atomic>
#include <boost/assert.hpp>
#include <co2/detail/task.hpp>

namespace co2 { namespace task_detail
{
    struct atomic_coroutine_handle
    {
        atomic_coroutine_handle() : _p{nullptr} {}

        atomic_coroutine_handle(atomic_coroutine_handle&&) = delete;

        coroutine<> steal(coroutine<>& coro)
        {
            return coroutine<>(_p.exchange(coro.detach(), std::memory_order_relaxed));
        }

        coroutine<> exchange_null()
        {
            return coroutine<>(_p.exchange(nullptr, std::memory_order_relaxed));
        }

        ~atomic_coroutine_handle()
        {
            coroutine<>(_p.load(std::memory_order_relaxed));
        }

        std::atomic<coroutine<>::handle_type> _p;
    };

    struct unique_promise_base : promise_base
    {
        atomic_coroutine_handle _then;
        std::atomic<tag> _tag {tag::null};
        std::atomic_flag _last_owner = ATOMIC_FLAG_INIT;

        bool test_last() noexcept
        {
            return _last_owner.test_and_set(std::memory_order_relaxed);
        }

        void finalize() noexcept
        {
            if (auto then = _then.exchange_null())
                then();
        }

        bool follow(coroutine<>& cb)
        {
            auto old = _then.steal(cb);
            BOOST_ASSERT_MSG(!old, "multiple coroutines await on same task");
            return _tag.load(std::memory_order_relaxed) == tag::null
                || !(cb = _then.exchange_null());
        }
    };
}}

namespace co2
{
    template<class T>
    struct task
      : task_detail::impl<task<T>, task_detail::unique_promise_base>
    {
        using base_type =
            task_detail::impl<task<T>, task_detail::unique_promise_base>;

        using base_type::base_type;

        task() = default;

        task(task&&) = default;

        task& operator=(task&& other) = default;

        T await_resume()
        {
            T ret(this->_promise->get());
            this->release();
            this->_promise = nullptr;
            return ret;
        }

        shared_task<T> share()
        {
            return task_detail::share(std::move(*this));
        }
    };

    template<class T>
    inline void swap(task<T>& a, task<T>& b) noexcept
    {
        a.swap(b);
    }
}

#endif