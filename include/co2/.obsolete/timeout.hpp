/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SYNC_TIMEOUT_HPP_INCLUDED
#define CO2_SYNC_TIMEOUT_HPP_INCLUDED

#include <atomic>
#include <memory>
#include <exception>
#include <co2/coroutine.hpp>

namespace co2
{
    namespace detail
    {
        struct timeout_op
        {
            coroutine_handle h;

            template<class E>
            void operator()(E const&)
            {
                coroutine<>{h}();
            }
        };

        template<class Task, class Timer>
        struct timed_state
        {
            std::atomic<coroutine_handle> coro;
            Task work;
            Timer& timer;
            bool is_timeout;

            template<class T>
            timed_state(T&& work, Timer& timer)
              : coro{nullptr}, work(std::forward<T>(work)), timer(timer)
            {}

            ~timed_state()
            {
                coroutine<>{coro.load(std::memory_order_relaxed)};
            }

            void set_ready(bool timeout)
            {
                if (auto then = coro.exchange(nullptr, std::memory_order_relaxed))
                {
                    is_timeout = timeout;
                    coroutine<>{then}();
                }
            }

            static auto make_task(timed_state& state)
            CO2_BEG(task<await_result_t<Task>>, (state), CO2_TEMP_SIZE(0);)
            {
                CO2_SUSPEND([&](coroutine<>& coro)
                {
                    state.coro.store(coro.detach(), std::memory_order_relaxed);
                });
                if (state.is_timeout)
                    throw std::system_error(std::make_error_code(std::errc::timed_out));
                state.timer.cancel();
                CO2_RETURN_FROM(state.work.await_resume());
            } CO2_END

            static auto wait_work(std::shared_ptr<timed_state> state)
            CO2_BEG(void, (state), CO2_TEMP_SIZE(0);)
            {
                CO2_SUSPEND(state->work.await_suspend);
                state->set_ready(false);
            } CO2_END

            static auto wait_timer(std::shared_ptr<timed_state> state)
            CO2_BEG(void, (state), CO2_TEMP_SIZE(0);)
            {
                CO2_SUSPEND([&](coroutine<>& coro)
                {
                    state->timer.async_wait(timeout_op{coro.detach()});
                });
                state->set_ready(true);
            } CO2_END
        };

        template<class Task, class Timer>
        auto timeout(Task&& work, Timer& timer, typename Timer::duration const& duration)
        {
            using state_t = timed_state<std::decay_t<Task>, Timer>;
            auto state = std::make_shared<state_t>(copy_or_move<Task>(work), timer);
            auto ret(state_t::make_task(*state));
            for (;;)
            {
                if (work.await_ready())
                {
                    state->set_ready(false);
                    break;
                }
                state_t::wait_work(state);
                if (!state->coro.load(std::memory_order_relaxed))
                    break;
                timer.expires_from_now(duration);
                state_t::wait_timer(state);
                break;
            }
            return ret;
        }
    }

    template<class Timer>
    class timeout
    {
        Timer _timer;

    public:
        template<class Arg>
        explicit timeout(Arg& arg) : _timer(arg) {}

        template<class Task>
        task<await_result_t<Task>> operator()(Task&& work, typename Timer::duration const& duration)
        {
            return detail::timeout(std::forward<Task>(work), _timer, duration);
        }
    };
}

#endif