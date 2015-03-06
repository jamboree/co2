/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_TASK_HPP_INCLUDED
#define CO2_TASK_HPP_INCLUDED

#include <atomic>
#include <vector>
#include <type_traits>
#include <co2/coroutine.hpp>
#include <co2/detail/task.hpp>

namespace co2 { namespace task_detail
{
    struct unique_promise_base : promise_base
    {
        atomic_coroutine_handle _then;
        std::atomic<tag> _tag {tag::null};

        void notify()
        {
            if (auto then = _then.exchange_null())
                then();
        }

        bool follow(coroutine<> const& cb)
        {
            auto old = _then.exchange(cb);
            BOOST_ASSERT_MSG(!old, "multiple coroutines await on same task");
            return _tag.load(std::memory_order_relaxed) == tag::null
                || !_then.exchange_null();
        }
    };
}}

namespace co2
{
    template<class T>
    struct shared_task;

    template<class T = void>
    struct task : task_detail::impl<task<T>, T, T, task_detail::unique_promise_base>
    {
        using base_type = task_detail::impl<task<T>, T, T, task_detail::unique_promise_base>;

        using base_type::base_type;

        task(task&&) = default;

        task& operator=(task&& other) = default;

        shared_task<T> share()
        {
            return [](task self) CO2_RET(shared_task<T>, (self))
            {
                CO2_AWAIT_RETURN(self);
            } CO2_END (std::move(*this));
        }
    };

    template<class T>
    inline void swap(task<T>& a, task<T>& b) noexcept
    {
        a.swap(b);
    }
}

#endif