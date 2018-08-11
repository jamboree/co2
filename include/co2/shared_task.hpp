/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2018 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SHARED_TASK_HPP_INCLUDED
#define CO2_SHARED_TASK_HPP_INCLUDED

#include <atomic>
#include <vector>
#include <co2/detail/task.hpp>

namespace co2 { namespace task_detail
{
    struct shared_promise_base : promise_base
    {
        std::atomic<void*> _then {this};
        std::atomic<unsigned> _use_count {2u};
        tag _tag {tag::pending};

        bool test_last() noexcept
        {
            return _use_count.fetch_sub(1u, std::memory_order_acquire) == 1u;
        }

        bool final_suspend() noexcept
        {
            auto next = _then.exchange(nullptr, std::memory_order_acq_rel);
            while (next != this)
            {
                auto then = static_cast<coroutine_handle>(next);
                next = coroutine_data(then);
                coroutine_final_run(then);
            }
            return _use_count.fetch_sub(1u, std::memory_order_release) != 1u;
        }

        bool follow(coroutine<>& cb)
        {
            auto curr = cb.handle();
            auto& next = coroutine_data(curr);
            next = _then.load(std::memory_order_acquire);
            while (next)
            {
                if (_then.compare_exchange_weak(next, curr, std::memory_order_release, std::memory_order_acquire))
                {
                    cb.detach();
                    return true;
                }
            }
            return false;
        }
    };
}}

namespace co2
{
    template<class T>
    struct shared_task
      : task_detail::impl<shared_task<T>, task_detail::shared_promise_base>
    {
        using base_type = task_detail::impl<shared_task<T>,
            task_detail::shared_promise_base>;

        using base_type::base_type;

        shared_task() = default;

        shared_task(shared_task&&) = default;

        shared_task(shared_task const& other) noexcept : base_type(other._promise)
        {
            if (auto promise = this->_promise)
                promise->_use_count.fetch_add(1u, std::memory_order_relaxed);
        }

        shared_task(task<T>&& other)
          : base_type(task_detail::convert<shared_task>(std::move(other)))
        {}

        shared_task& operator=(shared_task other) noexcept
        {
            this->~shared_task();
            return *new(this) shared_task(std::move(other));
        }

        task_detail::cref_t<T> await_resume()
        {
            return this->_promise->get();
        }
    };

    template<class T>
    inline void swap(shared_task<T>& a, shared_task<T>& b) noexcept
    {
        a.swap(b);
    }
}

#endif