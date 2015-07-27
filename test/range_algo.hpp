/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_TEST_RANGE_ALGO_HPP_INCLUDED
#define CO2_TEST_RANGE_ALGO_HPP_INCLUDED

template<class Gen>
void skip(Gen& gen, int n)
{
    if (!n)
        return;

    for (auto i : gen)
        if (!--n)
            break;
};

template<class Gen>
bool empty(Gen& gen)
{
    return gen.begin() == gen.end();
}

template<class Gen, class T>
bool equal_since(Gen& gen, T n)
{
    for (auto i : gen)
    {
        if (i != n)
            return false;
        ++n;
    }
    return true;
}

#endif