/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_DETAIL_FINAL_RESET_HPP_INCLUDED
#define CO2_DETAIL_FINAL_RESET_HPP_INCLUDED

namespace co2 { namespace detail
{
    template<class T>
    struct final_reset
    {
        T* that;

        ~final_reset()
        {
            that->reset();
        }
    };
}}

#endif