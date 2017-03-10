/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_DETAIL_COPY_OR_MOVE_HPP_INCLUDED
#define CO2_DETAIL_COPY_OR_MOVE_HPP_INCLUDED

#include <iterator>
#include <type_traits>

namespace co2 { namespace detail
{
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

    template<class T>
    struct copy_or_move_impl
    {
        using type = T&&;
    };

    template<class T>
    struct copy_or_move_impl<T&>
    {
        using type = std::conditional_t<
            std::is_copy_constructible<T>::value, T const&, T&&>;
    };

    template<class T>
    using copy_or_move_t = typename copy_or_move_impl<T>::type;

    template<class T>
    inline copy_or_move_t<T> copy_or_move(T& t)
    {
        return static_cast<copy_or_move_t<T>>(t);
    }
}}

#endif