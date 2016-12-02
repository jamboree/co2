/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_DETAIL_STORAGE_HPP_INCLUDED
#define CO2_DETAIL_STORAGE_HPP_INCLUDED

#include <exception>
#include <functional>
#include <type_traits>

namespace co2 { namespace detail
{
    template<class T>
    struct wrap_reference
    {
        using type = T;
    };

    template<class T>
    struct wrap_reference<T&>
    {
        using type = std::reference_wrapper<T>;
    };

    template<class T>
    using wrap_reference_t = typename wrap_reference<T>::type;

    enum class tag
    {
        null, value, exception, cancelled
    };

    template<class T>
    union storage
    {
        char data[1];
        T value;
        std::exception_ptr exception;

        storage() {}
        ~storage() {}

        void destroy(tag t) noexcept
        {
            switch (t)
            {
            case tag::value:
                value.~T();
                break;
            case tag::exception:
                exception.~exception_ptr();
            default:
                break;
            }
        }
    };
}}

#endif