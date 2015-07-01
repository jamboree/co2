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
        suspend_never initial_suspend()
        {
            return{};
        }

        void finalize() noexcept {}

        bool cancellation_requested() const
        {
            return _cancelled;
        }

        optional<T> get_return_object()
        {
            return std::move(_ret);
        }

        template<class T>
        void set_result(T val)
        {
            _ret = std::forward<T>(val);
        }

        void cancel()
        {
            _cancelled = true;
        }

    private:

        optional<T> _ret;
        bool _cancelled = false;
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
    inline bool await_suspend(optional<T> const& opt, co2::coroutine<co2::boost_optional_detail::promise<T>> const& cb)
    {
        cb.promise().cancel();
        return false;
    }

    template<class T>
    decltype(auto) await_resume(optional<T> const& opt)
    {
        return opt.get();
    }
}

#endif