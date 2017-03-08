/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SYNC_CRITICAL_SECTION_HPP_INCLUDED
#define CO2_SYNC_CRITICAL_SECTION_HPP_INCLUDED

#include <atomic>
#include <co2/coroutine.hpp>

namespace co2
{
    class critical_section
    {
        std::atomic<void*> _then{nullptr};

    public:
        struct leave_guard
        {
            critical_section& self;
            operator bool() const { return false; }
            ~leave_guard()
            {
                self.leave();
            }
        };

        bool enter() noexcept
        {
            void* curr = nullptr;
            return _then.compare_exchange_strong(curr, this, std::memory_order_relaxed);
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

        void do_suspend(coroutine<>& cb) noexcept
        {
            auto curr = cb.detach();
            auto& next = coroutine_data(curr);
            next = _then.load(std::memory_order_relaxed);
            while (!_then.compare_exchange_weak(next, curr, std::memory_order_release));
        }
    };
}

#define CO2_CRITICAL_SECTION(cs, ...)                                           \
do                                                                              \
{                                                                               \
    if (!cs.enter())                                                            \
        CO2_SUSPEND(cs.do_suspend);                                             \
    ::co2::critical_section::leave_guard _{cs};                                 \
    __VA_ARGS__                                                                 \
} while (false)                                                                 \
/***/

#endif