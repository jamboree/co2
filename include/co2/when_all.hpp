/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_WHEN_ALL_HPP_INCLUDED
#define CO2_WHEN_ALL_HPP_INCLUDED

#include <co2/task.hpp>
#include <co2/nth_ready.hpp>

namespace co2
{
    template<class... T>
    struct when_all_result
    {
        using type = std::tuple<typename await_result<T>::type...>;
    };

    template<class... T>
    using when_all_result_t = typename when_all_result<T...>::type;

    namespace detail
    {
        template<class... T, std::size_t... N>
        inline when_all_result_t<T...> make_all_result(std::tuple<T...>& args, std::index_sequence<N...>)
        {
            return when_all_result_t<T...>(await_resume(std::get<N>(args))...);
        }
    }

    template<class... T>
    inline auto when_all(std::tuple<T...> args)
    CO2_BEG(co2::task<when_all_result_t<T...>>, (args), int i;)
    {
        for (i = 0; i != sizeof...(T); ++i)
            CO2_AWAIT(nth_ready(i, args));
        CO2_RETURN(detail::make_all_result(args, std::make_index_sequence<sizeof...(T)>{}));
    } CO2_END

    template<class... T>
    inline auto when_all(T&&... t)
    {
        return when_all(std::tuple<T...>(std::forward<T>(t)...));
    }
}

#endif