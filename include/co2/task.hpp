/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2016 Jamboree

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
    struct unique_promise_base : promise_base
    {
        std::atomic<coroutine_handle> _then {nullptr};
        std::atomic<tag> _tag {tag::null};
        std::atomic_flag _last_owner = ATOMIC_FLAG_INIT;

        bool test_last() noexcept
        {
            return _last_owner.test_and_set(std::memory_order_relaxed);
        }

        void finalize() noexcept
        {
            if (auto then = _then.exchange(nullptr, std::memory_order_relaxed))
                coroutine_descend(then);
        }

        bool follow(coroutine<>& cb)
        {
            auto old = _then.exchange(cb.handle(), std::memory_order_relaxed);
            BOOST_ASSERT_MSG(!old, "multiple coroutines await on same task");
            if (_tag.load(std::memory_order_relaxed) == tag::null
                || !_then.exchange(nullptr, std::memory_order_relaxed))
            {
                cb.detach();
                return true;
            }
            return false;
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
            struct finalizer
            {
                task* that;
                ~finalizer()
                {
                    that->reset();
                }
            } _{this};
            return this->_promise->get();
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

    inline auto make_ready_task() CO2_BEG(task<>, ())
    {
        CO2_RETURN();
    } CO2_END

    template<class T>
    inline auto make_ready_task(T&& val) CO2_BEG(task<std::decay_t<T>>, (val))
    {
        CO2_RETURN(std::forward<T>(val));
    } CO2_END

    template<class T>
    inline auto make_ready_task(std::reference_wrapper<T> val) CO2_BEG(task<T&>, (val))
    {
        CO2_RETURN(val);
    } CO2_END

    template<class T>
    inline auto make_exceptional_task(std::exception_ptr ex) CO2_BEG(task<T>, (ex))
    {
        std::rethrow_exception(std::move(ex));
    } CO2_END

    template<class T = void>
    inline auto make_cancelled_task() CO2_BEG(task<T>, ())
    {
        CO2_AWAIT(co2::suspend_always{});
    } CO2_END

}

#endif