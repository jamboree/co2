/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_ADAPTED_BOOST_OPTIONAL_HPP_INCLUDED
#define CO2_ADAPTED_BOOST_OPTIONAL_HPP_INCLUDED

#include <co2/coroutine.hpp>
#include <boost/optional/optional.hpp>

namespace co2 { namespace boost_optional_detail
{
    using namespace boost;

    template<class T>
    struct promise
    {
        bool initial_suspend() noexcept
        {
            return false;
        }

        bool final_suspend() noexcept
        {
            return true;
        }

        bool cancellation_requested() const noexcept
        {
            return false;
        }

        optional<T> get_return_object(coroutine<promise>& coro)
        {
            coro.resume();
            auto ret(std::move(_ret));
            coroutine<promise>::destroy(this);
            return ret;
        }

        template<class T>
        void set_result(T val)
        {
            _ret = std::forward<T>(val);
        }

    private:

        optional<T> _ret;
    };
}}

namespace co2
{
    template<class T>
    struct coroutine_traits<boost::optional<T>>
    {
        using promise_type = boost_optional_detail::promise<T>;
    };
}

namespace boost
{
    template<class T>
    inline bool await_ready(optional<T> const& opt)
    {
        return !!opt;
    }

    template<class T>
    inline void await_suspend(optional<T> const& opt, co2::coroutine<>& coro)
    {
        coro.reset();
    }

    template<class T>
    decltype(auto) await_resume(optional<T> const& opt)
    {
        return opt.get();
    }
}

#endif