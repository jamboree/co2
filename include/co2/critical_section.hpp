/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_CRITICAL_SECTION_HPP_INCLUDED
#define CO2_CRITICAL_SECTION_HPP_INCLUDED

#include <atomic>
#include <co2/coroutine.hpp>

namespace co2
{
    class critical_section;

    namespace detail
    {
        struct critical_section
        {
            std::atomic<void*> _then{nullptr};

            friend class co2::critical_section;

            struct leave_
            {
                critical_section& self;
                ~leave_()
                {
                    self.leave();
                }
            };

            bool enter() noexcept
            {
                void* curr = nullptr;
                void* next = this;
                return _then.compare_exchange_strong(curr, next, std::memory_order_relaxed);
            }

            void leave() noexcept
            {
                void* curr = this;
                void* next = nullptr;
                if (_then.compare_exchange_strong(curr, next, std::memory_order_acq_rel))
                    return;
                do
                {
                    next = coroutine_data(static_cast<coroutine_handle>(curr));
                } while (!_then.compare_exchange_weak(curr, next, std::memory_order_acq_rel));
                coroutine<>{static_cast<coroutine_handle>(curr)}();
            }

            bool do_suspend(coroutine<>& cb) noexcept
            {
                auto prev = _then.load(std::memory_order_relaxed);
                auto curr = cb.detach();
                auto& next = coroutine_data(curr);
                do
                {
                    next = prev;
                } while (!_then.compare_exchange_weak(prev, curr, std::memory_order_release));
                return true;
            }
        };
    }

    class critical_section
    {
        template<class F>
        struct awaiter
        {
            detail::critical_section& self;
            F f;
            bool const ready;

            bool await_ready() noexcept
            {
                return ready;
            }

            bool await_suspend(coroutine<>& cb) noexcept
            {
                return self.do_suspend(cb);
            }

            decltype(auto) await_resume()
            {
                detail::critical_section::leave_ _{self};
                return f();
            }
        };

        friend detail::critical_section& get_impl(critical_section& cs) noexcept
        {
            return cs._impl;
        }

        detail::critical_section _impl;

    public:
        template<class F>
        awaiter<F> operator()(F f)
        {
            return {_impl, std::move(f), enter()};
        }
    };
}

#define CO2_CRITICAL_SECTION(cs, ...)                                           \
do                                                                              \
{                                                                               \
    if (!get_impl(cs).enter())                                                  \
        CO2_YIELD_WITH(get_impl(cs).do_suspend);                                \
    ::co2::detail::critical_section::leave_ _{get_impl(cs)};                    \
    __VA_ARGS__                                                                 \
} while (false)                                                                 \
/***/

#endif