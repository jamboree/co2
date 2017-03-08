/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SYNC_MULTIPLEXER_HPP_INCLUDED
#define CO2_SYNC_MULTIPLEXER_HPP_INCLUDED

#include <atomic>
#include <memory>
#include <co2/task.hpp>
#include <co2/detail/copy_or_move.hpp>

namespace co2 { namespace detail
{
    template<class Task>
    struct multiplexer_state
    {
        Task result;
        void* active_list = nullptr;
        std::atomic<void*> ready_list{this};
        coroutine_handle then = nullptr;
        std::atomic_flag lock_flag = ATOMIC_FLAG_INIT;

        multiplexer_state()
        {
            lock_flag.test_and_set(std::memory_order_relaxed);
        }

        ~multiplexer_state()
        {
            kill_list(active_list);
            auto next = ready_list.load(std::memory_order_relaxed);
            if (next != this)
                kill_list(next);
        }

        void kill_list(void* next)
        {
            while (next)
            {
                auto then = static_cast<coroutine_handle>(next);
                next = coroutine_data(then);
                coroutine<>{then};
                if (next == this)
                    break;
            }
        }

        void set_result(Task&& t)
        {
            result = std::move(t);
            if (auto next = then)
            {
                then = nullptr;
                coroutine<>{next}();
            }
        }

        coroutine_handle pop_active()
        {
            auto ret = static_cast<coroutine_handle>(active_list);
            if (ret)
            {
                active_list = coroutine_data(ret);
                if (active_list == this)
                    active_list = nullptr;
            }
            return ret;
        }

        bool add_ready(coroutine<>& coro)
        {
            auto h = coro.handle();
            auto& next = coroutine_data(h);
            next = nullptr;
            if (ready_list.compare_exchange_strong(next, this, std::memory_order_relaxed))
            {
                if (lock())
                    return false;
                next = this;
            }
            while (!ready_list.compare_exchange_weak(next, h, std::memory_order_relaxed))
            {
                if (!next && lock())
                    return false;
            }
            coro.detach();
            return true;
        }

        bool lock()
        {
            return !lock_flag.test_and_set(std::memory_order_acquire);
        }

        bool suspend(coroutine<>& coro)
        {
            then = coro.handle();
            lock_flag.clear(std::memory_order_release);
            void* sentinel = this;
            if (!ready_list.compare_exchange_strong(sentinel, nullptr, std::memory_order_relaxed) && sentinel)
            {
                then = nullptr;
                active_list = ready_list.exchange(this, std::memory_order_relaxed);
                return false;
            }
            coro.detach();
            return true;
        }

        static auto wait_task(Task t, std::weak_ptr<multiplexer_state> state)
        CO2_BEG(void, (t, state), CO2_TEMP_SIZE(0);)
        {
            if (!t.await_ready())
            {
                CO2_SUSPEND(t.await_suspend);
            }
            CO2_SUSPEND([&](coroutine<>& coro)
            {
                if (auto p = state.lock())
                    return p->add_ready(coro);
                return true;
            });
            if (auto p = state.lock())
                p->set_result(std::move(t));
        } CO2_END
    };
}}

namespace co2
{
    template<class Task>
    class multiplexer
    {
        using state_t = detail::multiplexer_state<Task>;

        struct awaiter
        {
            state_t& state;

            bool await_ready() const
            {
                return !!state.active_list;
            }

            bool await_suspend(coroutine<>& coro)
            {
                return state.suspend(coro);
            }

            await_result_t<Task> await_resume()
            {
                if (auto coro = state.pop_active())
                    coroutine<>{coro}();
                return state.result.await_resume();
            }
        };

        std::shared_ptr<state_t> _state;
        std::size_t _count;

    public:
        multiplexer()
          : _state(std::make_shared<state_t>()), _count(0)
        {}

        void add(Task t)
        {
            state_t::wait_task(std::move(t), _state);
            ++_count;
        }

        template<class InputIt>
        void add(InputIt first, InputIt last)
        {
            for (; first != last; ++first)
            {
                state_t::wait_task(detail::copy_or_move(*first), _state);
                ++_count;
            }
        }

        explicit operator bool() const
        {
            return !!_count;
        }

        awaiter select()
        {
            --_count;
            return {*_state};
        }
    };
}

#endif