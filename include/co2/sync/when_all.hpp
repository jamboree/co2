/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SYNC_WHEN_ALL_HPP_INCLUDED
#define CO2_SYNC_WHEN_ALL_HPP_INCLUDED

#include <tuple>
#include <vector>
#include <co2/task.hpp>
#include <co2/detail/copy_or_move.hpp>

namespace co2
{
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

            constexpr await_table_t()
              : await_table_t(std::make_index_sequence<size>{})
            {}

            template<std::size_t... N>
            constexpr await_table_t(std::index_sequence<N...>)
              : ready{ready_fn<N>...}
              , suspend{suspend_fn<N>...}
            {}

            bool(*ready[size])(Tuple& t);
            bool(*suspend[size])(Tuple& t, coroutine<>& coro);
        };

        template<class Tuple>
        await_table_t<Tuple> const await_table = {};

        template<class T>
        auto when_all_impl(std::vector<T> seq) CO2_BEG(task<std::vector<T>>, (seq),
            CO2_TEMP_SIZE(0);
            CO2_AUTO(it, seq.begin());
        )
        {
            for (; it != seq.end(); ++it)
            {
                if (!it->await_ready())
                {
                    CO2_SUSPEND(it->await_suspend);
                }
            }
            CO2_RETURN_LOCAL(seq);
        } CO2_END

        template<class... T>
        auto when_all_impl(std::tuple<T...> seq) CO2_BEG(task<std::tuple<T...>>, (seq),
            CO2_TEMP_SIZE(0);
            std::size_t i = 0;
        )
        {
            using seq_t = std::tuple<T...>;
            for (; i != std::tuple_size<seq_t>::value; ++i)
            {
                if (!await_table<seq_t>.ready[i](seq))
                {
                    CO2_SUSPEND([&](coroutine<>& coro)
                    {
                        return await_table<seq_t>.suspend[i](seq, coro);
                    });
                }
            }
            CO2_RETURN_LOCAL(seq);
        } CO2_END
    }

    template<class InputIt>
    inline auto when_all(InputIt first, InputIt last) ->
        task<std::vector<typename std::iterator_traits<InputIt>::value_type>>
    {
        using task_t = typename std::iterator_traits<InputIt>::value_type;
        using seq_t = std::vector<task_t>;
        using iter = detail::copy_or_move_iter<InputIt, std::is_copy_constructible<task_t>::value>;
        return detail::when_all_impl(seq_t(iter::wrap(first), iter::wrap(last)));
    }

    template<class... Futures>
    inline auto when_all(Futures&&... futures) ->
        task<std::tuple<std::decay_t<Futures>...>>
    {
        using seq_t = std::tuple<std::decay_t<Futures>...>;
        return detail::when_all_impl(seq_t(detail::copy_or_move<Futures>(futures)...));
    }
}

#endif