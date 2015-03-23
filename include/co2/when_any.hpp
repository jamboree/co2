/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_WHEN_ANY_HPP_INCLUDED
#define CO2_WHEN_ANY_HPP_INCLUDED

#include <atomic>
#include <co2/task.hpp>
#include <co2/utility/ornion.hpp>

namespace co2 { namespace detail
{
    template<class... T>
    struct when_any_context
    {
        using result_t = ornion<typename await_result<T>::type...>;
        static constexpr std::size_t tuple_size = sizeof...(T);

        result_t result;
        std::atomic_flag done = ATOMIC_FLAG_INIT;

        template<std::size_t N, class U>
        void set_from(U& u)
        {
            try
            {
                set_value<N>(result, (await_resume(u), void_{}));
            }
            catch (...)
            {
                set_exception<N>(result, std::current_exception());
            }
        }

        template<std::size_t N>
        std::enable_if_t<(tuple_size == N + 1), bool>
        is_ready(std::tuple<T...>& t)
        {
            auto& u = std::get<N>(t);
            if (await_ready(u))
            {
                set_from<N>(u);
                return true;
            }
            return false;
        }

        template<std::size_t N>
        std::enable_if_t<(tuple_size != N + 1), bool>
        is_ready(std::tuple<T...>& t)
        {
            auto& u = std::get<N>(t);
            if (await_ready(u))
            {
                set_from<N>(u);
                return true;
            }
            return is_ready<N + 1>(t);
        }

        template<std::size_t N, class U>
        static auto any_then(when_any_context& ctx, U& u, coroutine<> then) CO2_RET(coroutine<>, (ctx, u, then))
        {
            CO2_AWAIT(suspend_always{});
            if (!ctx.done.test_and_set(std::memory_order_relaxed))
            {
                ctx.set_from<N>(u);
                then();
            }
        } CO2_END

        template<std::size_t N>
        std::enable_if_t<(tuple_size == N + 1), bool>
        try_suspend(std::tuple<T...>& t, coroutine<> const& then)
        {
            auto& u = std::get<N>(t);
            return await_suspend(u, any_then<N>(*this, u, then)), void_{};
        }

        template<std::size_t N>
        std::enable_if_t<(tuple_size != N + 1), bool>
        try_suspend(std::tuple<T...>& t, coroutine<> const& then)
        {
            auto& u = std::get<N>(t);
            return (await_suspend(u, any_then<N>(*this, u, then)), void_{}) && try_suspend<N + 1>(t, then);
        }

        auto suspend(std::tuple<T...>& t)
        {
            struct awaiter
            {
                when_any_context& ctx;
                std::tuple<T...>& t;

                bool await_ready()
                {
                    return ctx.is_ready<0>(t);
                }

                bool await_suspend(coroutine<> const& then)
                {
                    return ctx.try_suspend<0>(t, then);
                }

                result_t&& await_resume()
                {
                    return std::move(ctx.result);
                }
            };
            return awaiter{*this, t};
        }
    };
}}

namespace co2
{
    template<class... T>
    inline auto when_any(std::tuple<T...> t)
    CO2_RET(task<typename detail::when_any_context<T...>::result_t>, (t),
        detail::when_any_context<T...> ctx;
    )
    {
        CO2_AWAIT_RETURN(ctx.suspend(t));
    } CO2_END

    template<class... T>
    inline auto when_any(T&&... t)
    {
        return when_any(std::tuple<T...>(std::forward<T>(t)...));
    }
}

#endif