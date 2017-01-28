/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_EVENT_HPP_INCLUDED
#define CO2_EVENT_HPP_INCLUDED

#include <atomic>
#include <co2/coroutine.hpp>

namespace co2
{
    class event
    {
        std::atomic<void*> _then{this};

    public:
        void set() noexcept
        {
            auto next = _then.exchange(nullptr, std::memory_order_acquire);
            while (next != this)
            {
                auto then = static_cast<coroutine_handle>(next);
                next = coroutine_data(then);
                coroutine<>{then}();
            }
        }

        bool await_ready() noexcept
        {
            return !_then.load(std::memory_order_relaxed);
        }

        bool await_suspend(coroutine<>& cb) noexcept
        {
            auto prev = _then.load(std::memory_order_relaxed);
            auto curr = cb.to_address();
            auto& next = coroutine_data(cb.handle());
            while (prev)
            {
                next = prev;
                if (_then.compare_exchange_weak(prev, curr, std::memory_order_release))
                {
                    cb.detach();
                    return true;
                }
            }
            return false;
        }

        void await_resume() noexcept {}
    };
}

#endif