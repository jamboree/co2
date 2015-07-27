/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_TEST_TRIGGER_HPP_INCLUDED
#define CO2_TEST_TRIGGER_HPP_INCLUDED

#include <co2/coroutine.hpp>

template<class T>
struct trigger
{
    co2::coroutine<> _coro;
    T _val;

    bool await_ready() noexcept
    {
        return false;
    }

    void await_suspend(co2::coroutine<>& coro)
    {
        _coro = std::move(coro);
    }

    T await_resume()
    {
        return _val;
    }

    void operator()(T val)
    {
        _val = val;
        if (_coro)
            _coro();
    }

    void cancel()
    {
        _coro.reset();
    }
};

#endif