/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SYNC_WHEN_ANY_HPP_INCLUDED
#define CO2_SYNC_WHEN_ANY_HPP_INCLUDED

#include <atomic>
#include <vector>
#include <memory>
#include <iterator>
#include <co2/coroutine.hpp>

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
        template<class Tuple>
        struct await_table_t
        {
            static constexpr std::size_t size = std::tuple_size<Tuple>::value;

            template<std::size_t N>
            static bool ready_fn(Tuple& t)
            {
                return co2::await_ready(std::get<N>(t));
            }

            template<std::size_t N>
            static bool suspend_fn(Tuple& t, coroutine<>& coro)
            {
                return co2::await_suspend(std::get<N>(t), coro);
            }

            template<std::size_t... N>
            constexpr await_table_t(std::index_sequence<N...> = {})
              : ready{ready_fn<N>...}
              , suspend{suspend_fn<N>...}
            {}

            bool(*ready[size])(Tuple& t);
            bool(*suspend[size])(Tuple& t, coroutine<>& coro);
        };

        template<class Tuple>
        constexpr await_table_t<Tuple> await_table = {};

        template<class T>
        inline bool ready_at(std::vector<T>& futures, std::size_t i)
        {
            return futures[i].await_ready();
        }

        template<class T>
        inline bool suspend_at(std::vector<T>& futures, std::size_t i, coroutine<>& coro)
        {
            return futures[i].await_suspend(coro);
        }

        template<class... T>
        inline bool ready_at(std::tuple<T...>& futures, std::size_t i)
        {
            return await_table<std::tuple<T...>>.ready[i](futures);
        }

        template<class... T>
        inline bool suspend_at(std::tuple<T...>& futures, std::size_t i, coroutine<>& coro)
        {
            return await_table<std::tuple<T...>>.suspend[i](futures, coro);
        }

        template<class State>
        auto wait_any_at(std::size_t i, std::shared_ptr<State> state)
        CO2_BEG(void, (i, state), CO2_TEMP_SIZE(0);)
        {
            if (!ready_at(state->result.futures, i))
            {
                CO2_SUSPEND([&](coroutine<>& coro)
                {
                    return suspend_at(state->result.futures, i, coro);
                });
            }
            state->set_ready(i);
        } CO2_END

        template<class Sequence>
        struct when_any_state
        {
            when_any_result<Sequence> result;
            std::atomic<coroutine_handle> coro;

            void set_ready(std::size_t i)
            {
                if (auto then = coro.exchange(nullptr, std::memory_order_relaxed))
                {
                    result.index = i;
                    coroutine<>{then}();
                }
            }

            ~when_any_state()
            {
                coroutine<>{coro.load(std::memory_order_relaxed)};
            }

            static auto make_task(when_any_state& state) CO2_BEG(task<when_any_result<Sequence>>, (state))
            {
                CO2_SUSPEND([&](coroutine<>& coro)
                {
                    state.coro.store(coro.detach(), std::memory_order_relaxed);
                });
                CO2_RETURN(std::move(state.result));
            } CO2_END
        };

        template<class It, bool copy>
        struct copy_or_move_iter
        {
            static It const& wrap(It const& it)
            {
                return it;
            }
        };

        template<class It>
        struct copy_or_move_iter<It, false>
        {
            static std::move_iterator<It> wrap(It const& it)
            {
                return std::move_iterator<It>(it);
            }
        };
    }

    template<class InputIt>
    auto when_any(InputIt first, InputIt last) ->
        task<when_any_result<std::vector<typename std::iterator_traits<InputIt>::value_type>>>
    {
        using task_t = typename std::iterator_traits<InputIt>::value_type;
        using seq_t = std::vector<task_t>;
        auto state = std::make_shared<detail::when_any_state<seq_t>>();
        auto ret(detail::when_any_state<seq_t>::make_task(*state));
        using iter = detail::copy_or_move_iter<InputIt, std::is_copy_constructible<task_t>::value>;
        state->result.futures.assign(iter::wrap(first), iter::wrap(last));
        if (std::size_t n = state->result.futures.size())
        {
            for (std::size_t i = 0; i != n; ++i)
            {
                detail::wait_any_at(i, state);
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
        auto state = std::make_shared<detail::when_any_state<seq_t>>();
        auto ret(detail::when_any_state<seq_t>::make_task(*state));
        state->result.futures = std::forward_as_tuple(std::forward<Futures>(futures)...);
        if (constexpr std::size_t n = sizeof...(Futures))
        {
            for (std::size_t i = 0; i != n; ++i)
            {
                detail::wait_any_at(i, state);
                if (!state->coro.load(std::memory_order_relaxed))
                    break;
            }
        }
        else
            state->set_ready(std::size_t(-1));
        return ret;
    }
}

#endif