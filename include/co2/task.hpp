/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2018 Jamboree

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
        std::atomic<void*> _then {this};
        tag _tag {tag::pending};

        bool test_last() noexcept
        {
            return !_then.exchange(nullptr, std::memory_order_acquire);
        }

        bool final_suspend() noexcept
        {
            auto then = _then.exchange(nullptr, std::memory_order_acq_rel);
            if (then != this)
            {
                if (!then) // Task is destroyed, we're the last owner.
                    return false;
                coroutine_final_run(static_cast<coroutine_handle>(then));
            }
            return true; // We're done. Let the task do the cleanup.
        }

        bool follow(coroutine<>& cb)
        {
            void* last = this;
            if (_then.compare_exchange_strong(last, cb.handle(), std::memory_order_release, std::memory_order_acquire))
            {
                cb.detach();
                return true;
            }
            // If there's a previous waiter, just cancel it because it's only
            // allowed for when_any.
            if (last)
            {
                if (_then.compare_exchange_strong(last, cb.handle(), std::memory_order_acq_rel))
                {
                    coroutine<>{static_cast<coroutine_handle>(last)};
                    cb.detach();
                    return true;
                }
                BOOST_ASSERT_MSG(!last, "multiple coroutines await on same task");
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
            return detail::extract_promise<task>{this->_promise}->get();
        }

        shared_task<T> share()
        {
            return task_detail::convert<shared_task<T>>(std::move(*this));
        }
    };

    template<class T>
    inline void swap(task<T>& a, task<T>& b) noexcept
    {
        a.swap(b);
    }

    inline auto make_ready_task() CO2_BEG(task<>, (), CO2_TEMP_SIZE(0);)
    {
        CO2_RETURN();
    } CO2_END

    template<class T>
    inline auto make_ready_task(T&& val) CO2_BEG(task<std::decay_t<T>>, (val), CO2_TEMP_SIZE(0);)
    {
        CO2_RETURN(std::forward<T>(val));
    } CO2_END

    template<class T>
    inline auto make_ready_task(std::reference_wrapper<T> val) CO2_BEG(task<T&>, (val), CO2_TEMP_SIZE(0);)
    {
        CO2_RETURN(val);
    } CO2_END

    template<class T>
    inline auto make_exceptional_task(std::exception_ptr ex) CO2_BEG(task<T>, (ex), CO2_TEMP_SIZE(0);)
    {
        std::rethrow_exception(std::move(ex));
    } CO2_END

    template<class T = void>
    inline auto make_cancelled_task() CO2_BEG(task<T>, (), CO2_TEMP_SIZE(0);)
    {
#define Zz_CO2_CANCEL_OP(coro) true // suspend
        CO2_SUSPEND(Zz_CO2_CANCEL_OP);
#undef Zz_CO2_CANCEL_OP
    } CO2_END
}

#endif