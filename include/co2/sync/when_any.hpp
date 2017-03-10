/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SYNC_WHEN_ANY_HPP_INCLUDED
#define CO2_SYNC_WHEN_ANY_HPP_INCLUDED

#include <tuple>
#include <atomic>
#include <vector>
#include <memory>
#include <co2/task.hpp>
#include <co2/detail/copy_or_move.hpp>

namespace co2
{ 
    template<class Sequence>
    struct when_any_result
    {
        std::size_t index;
        Sequence futures;
    };

    namespace detail
    {
        template<class State>
        auto wait_any_at(std::size_t i, std::shared_ptr<State> state)
        CO2_BEG(void, (i, state), CO2_TEMP_SIZE(0);)
        {
            CO2_SUSPEND(state->result.futures[i].await_suspend);
            state->set_ready(i);
        } CO2_END
            
        template<std::size_t I, class State>
        auto wait_any_at(std::integral_constant<std::size_t, I>, std::shared_ptr<State> state)
        CO2_BEG(void, (state), CO2_TEMP_SIZE(0);)
        {
            CO2_SUSPEND(std::get<I>(state->result.futures).await_suspend);
            state->set_ready(I);
        } CO2_END

        template<class Sequence>
        struct when_any_state
        {
            std::atomic<coroutine_handle> coro;
            when_any_result<Sequence> result;

            template<class... T>
            when_any_state(bool/*dummy*/, T&&... t)
              : coro{nullptr}, result{0, {std::forward<T>(t)...}}
            {}

            ~when_any_state()
            {
                coroutine<>{coro.load(std::memory_order_relaxed)};
            }

            void set_ready(std::size_t i)
            {
                if (auto then = coro.exchange(nullptr, std::memory_order_relaxed))
                {
                    result.index = i;
                    coroutine<>{then}();
                }
            }

            static auto make_task(when_any_state& state)
            CO2_BEG(task<when_any_result<Sequence>>, (state), CO2_TEMP_SIZE(0);)
            {
                CO2_SUSPEND([&](coroutine<>& coro)
                {
                    state.coro.store(coro.detach(), std::memory_order_relaxed);
                });
                CO2_RETURN(std::move(state.result));
            } CO2_END
        };

        template<class State>
        inline void wait_any_each(State const& state, std::integral_constant<std::size_t, 0>, std::integral_constant<std::size_t, 0>)
        {
            state->set_ready(std::size_t(-1));
        }

        template<class State, std::size_t I, std::size_t E>
        inline void wait_any_each(State const& state, std::integral_constant<std::size_t, I> i, std::integral_constant<std::size_t, E> e)
        {
            if (std::get<I>(state->result.futures).await_ready())
                return state->set_ready(I);
            wait_any_at(i, state);
            if (state->coro.load(std::memory_order_relaxed))
                wait_any_each(state, std::integral_constant<std::size_t, I + 1>{}, e);
        }

        template<class State, std::size_t I>
        inline void wait_any_each(State const&, std::integral_constant<std::size_t, I>, std::integral_constant<std::size_t, I>) {}
    }

    template<class InputIt>
    auto when_any(InputIt first, InputIt last) ->
        task<when_any_result<std::vector<typename std::iterator_traits<InputIt>::value_type>>>
    {
        using task_t = typename std::iterator_traits<InputIt>::value_type;
        using seq_t = std::vector<task_t>;
        using iter = detail::copy_or_move_iter<InputIt, std::is_copy_constructible<task_t>::value>;
        auto state = std::make_shared<detail::when_any_state<seq_t>>(true, iter::wrap(first), iter::wrap(last));
        auto ret(detail::when_any_state<seq_t>::make_task(*state));
        if (const std::size_t n = state->result.futures.size())
        {
            for (std::size_t i = 0; i != n; ++i)
            {
                if (state->result.futures[i].await_ready())
                {
                    state->set_ready(i);
                    break;
                }
                wait_any_at(i, state);
                if (!state->coro.load(std::memory_order_relaxed))
                    break;
            }
        }
        else
            state->set_ready(std::size_t(-1));
        return ret;
    }

    template<class... Futures>
    auto when_any(Futures&&... futures) ->
        task<when_any_result<std::tuple<std::decay_t<Futures>...>>>
    {
        using seq_t = std::tuple<std::decay_t<Futures>...>;
        auto state = std::make_shared<detail::when_any_state<seq_t>>(true, detail::copy_or_move<Futures>(futures)...);
        auto ret(detail::when_any_state<seq_t>::make_task(*state));
        detail::wait_any_each(state, std::integral_constant<std::size_t, 0>{}, std::tuple_size<seq_t>{});
        return ret;
    }
}

#endif