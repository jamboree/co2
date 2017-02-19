/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SYNC_NTH_READY_HPP_INCLUDED
#define CO2_SYNC_NTH_READY_HPP_INCLUDED

#include <tuple>
#include <type_traits>
#include <co2/coroutine.hpp>

namespace co2 { namespace detail
{
    template<class Tuple>
    struct nth_ready_awaiter
    {
        Tuple args;
        int n;
        
        using args_t = std::remove_reference_t<Tuple>;
        static constexpr std::size_t tuple_size = std::tuple_size<args_t>::value;
        using indices_t = std::make_index_sequence<tuple_size>;

        template<std::size_t N>
        static bool await_ready_fn(args_t& args)
        {
            return co2::await_ready(std::get<N>(args));
        }

        template<std::size_t N, class F>
        static bool await_suspend_fn(args_t& args, F& f)
        {
            return co2::await_suspend(std::get<N>(args), f), void_{};
        }

        template<std::size_t... N>
        static auto await_ready_fn_table(std::index_sequence<N...>)
        {
            using fptr = bool(*)(args_t&);
            static fptr fs[tuple_size] =
            {
                await_ready_fn<N>...
            };
            return fs;
        }

        template<class F, std::size_t... N>
        static auto await_suspend_fn_table(std::index_sequence<N...>)
        {
            using fptr = bool(*)(args_t&, F&);
            static fptr fs[tuple_size] =
            {
                await_suspend_fn<N, F>...
            };
            return fs;
        }

        nth_ready_awaiter(Tuple&& args, int n) : args(std::forward<Tuple>(args)), n(n) {}

        bool await_ready()
        {
            return await_ready_fn_table(indices_t{})[n](args);
        }

        template<class F>
        bool await_suspend(F& then)
        {
            return await_suspend_fn_table<F>(indices_t{})[n](args, then);
        }

        void await_resume() const noexcept {}
    };
}}

namespace co2
{
    template<class Tuple>
    inline detail::nth_ready_awaiter<Tuple> nth_ready(int n, Tuple&& args)
    {
        return {std::forward<Tuple>(args), n};
    }
}

#endif