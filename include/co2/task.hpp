/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_TASK_HPP_INCLUDED
#define CO2_TASK_HPP_INCLUDED

#include <atomic>
#include <boost/assert.hpp>
#include <co2/detail/task.hpp>
#include <co2/detail/final_reset.hpp>

namespace co2 { namespace task_detail
{
    struct unique_promise_base : promise_base
    {
        std::atomic<void*> _then {this};
        std::atomic<tag> _tag {tag::null};
        std::atomic_flag _last_owner = ATOMIC_FLAG_INIT;

        bool test_last() noexcept
        {
            return _last_owner.test_and_set(std::memory_order_relaxed);
        }

        void finalize() noexcept
        {
            auto then = _then.exchange(nullptr, std::memory_order_relaxed);
            if (then != this)
                coroutine_final_run(static_cast<coroutine_handle>(then));
        }

        bool follow(coroutine<>& cb)
        {
            void* last = this;
            if (_then.compare_exchange_strong(last, cb.handle(), std::memory_order_relaxed))
            {
                cb.detach();
                return true;
            }
            // If there's a previous waiter, just cancel it because it's only
            // allowed for when_any.
            if (last)
            {
                if (_then.compare_exchange_strong(last, cb.handle(), std::memory_order_relaxed))
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
            detail::final_reset<task> _{this};
            return this->_promise->get();
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