/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_TEST_COMMON_HPP_INCLUDED
#define CO2_TEST_COMMON_HPP_INCLUDED

struct ball {};

struct inc_on_finalize
{
    int& count;
    ~inc_on_finalize() { ++count; }
};

#endif