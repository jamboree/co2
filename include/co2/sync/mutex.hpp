/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2018 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SYNC_MUTEX_HPP_INCLUDED
#define CO2_SYNC_MUTEX_HPP_INCLUDED

#include <atomic>
#include <boost/assert.hpp>
#include <co2/coroutine.hpp>

namespace co2
{
    class mutex
    {
        std::atomic<void*> _then;

    public:
        constexpr mutex() : _then{nullptr} {}

        // Non-copyable.
        mutex(mutex const&) = delete;
        mutex& operator=(mutex const&) = delete;

        ~mutex() { BOOST_ASSERT_MSG(!_then.load(), "mutex is not released"); }

        bool try_lock() noexcept
        {
            void* curr = nullptr;
            return _then.compare_exchange_strong(curr, this, std::memory_order_acquire, std::memory_order_relaxed);
        }

        bool lock_suspend(coroutine<>& coro) noexcept
        {
            auto& next = coroutine_data(coro.handle());
            next = _then.load(std::memory_order_relaxed);
            for (;;)
            {
                // If not locked, try to lock it.
                if (!next && _then.compare_exchange_strong(next, this, std::memory_order_acquire, std::memory_order_relaxed))
                    return false;
                BOOST_ASSERT(next);
                // Already locked, try to enqueue the coroutine.
                if (_then.compare_exchange_weak(next, coro.handle(), std::memory_order_acq_rel, std::memory_order_relaxed))
                    break;
            }
            coro.detach();
            return true;
        }

        void unlock() noexcept
        {
            void* curr = this;
            void* next = nullptr;
            // No others waiting, we're done.
            if (_then.compare_exchange_strong(curr, next, std::memory_order_release, std::memory_order_acquire))
                return;
            // Wake up next waiting coroutine.
            do
            {
                next = coroutine_data(static_cast<coroutine_handle>(curr));
            } while (!_then.compare_exchange_weak(curr, next, std::memory_order_acq_rel));
            coroutine<>{static_cast<coroutine_handle>(curr)}();
        }
    };

    template<class Lock>
    struct unlock_guard
    {
        Lock& lock;

        explicit unlock_guard(Lock& lock) : lock(lock) {}
        unlock_guard(unlock_guard const&) = delete;
        unlock_guard& operator=(unlock_guard const&) = delete;

        ~unlock_guard()
        {
            lock.unlock();
        }
    };

    template<class Lock>
    struct lock_awaiter
    {
        Lock& lock;

        bool await_ready() const noexcept
        {
            return lock.try_lock();
        }

        bool await_suspend(co2::coroutine<>& coro) const noexcept
        {
            return lock.lock_suspend(coro);
        }

        void await_resume() const noexcept {}
    };

    template<class Lock>
    inline lock_awaiter<Lock> make_lock(Lock& lock)
    {
        return {lock};
    }
}

#endif