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
#include <co2/lazy_task.hpp>

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
        auto wait_any_at(std::size_t i, std::shared_ptr<State> state) CO2_BEG(lazy_task<>, (i, state))
        {
            CO2_AWAIT(ready(state->result.futures[i]));
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
        auto state = std::make_shared<detail::when_any_state<std::vector<task_t>>>();
        auto ret(detail::when_any_state<std::vector<task_t>>::make_task(*state));
        using iter = detail::copy_or_move_iter<InputIt, std::is_copy_constructible<task_t>::value>;
        state->result.futures.assign(iter::wrap(first), iter::wrap(last));
        if (state->result.futures.empty())
            state->set_ready(std::size_t(-1));
        else
        {
            std::size_t i = 0;
            for (task_t& t : state->result.futures)
            {
                detail::wait_any_at(i, state);
                if (!state->coro.load(std::memory_order_relaxed))
                    break;
                ++i;
            }
        }
        return ret;
    }
}

#endif