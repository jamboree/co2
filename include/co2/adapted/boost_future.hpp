/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_ADAPTED_BOOST_FUTURE_HPP_INCLUDED
#define CO2_ADAPTED_BOOST_FUTURE_HPP_INCLUDED

#include <co2/coroutine.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/future.hpp>

namespace boost
{
    template<class T>
    inline bool await_ready(future<T>& fut)
    {
        return fut.is_ready();
    }

    template<class T>
    inline void await_suspend(future<T>& fut, ::co2::coroutine<>& cb)
    {
        thread([&fut, cb=std::move(cb)] mutable
        {
            fut.wait();
            cb();
        }).detach();
    }

    template<class T>
    inline T await_resume(future<T>& fut)
    {
        return fut.get();
    }
}

namespace co2 { namespace boost_future_detail
{
    using namespace boost;

    template<class T>
    struct promise_base
    {
        promise<T> promise;

        future<T> get_return_object(coroutine<>& coro)
        {
            auto ret(promise.get_future());
            coro();
            return ret;
        }

        bool initial_suspend() noexcept
        {
            return false;
        }

        bool final_suspend() noexcept
        {
            return false;
        }

        bool cancellation_requested() const
        {
            return false;
        }

        void set_exception(std::exception_ptr const& e)
        {
            promise.set_exception(e);
        }
    };
}}

namespace co2
{
    template<class T>
    struct coroutine_traits<boost::future<T>>
    {
        struct promise_type : boost_future_detail::promise_base<T>
        {
            void set_result(T&& t)
            {
                set_result<T>(std::forward<T>(t));
            }

            template<class U>
            void set_result(U&& u)
            {
                this->promise.set_value(std::forward<U>(u));
            }
        };
    };

    template<>
    struct coroutine_traits<boost::future<void>>
    {
        struct promise_type : boost_future_detail::promise_base<void>
        {
            void set_result()
            {
                this->promise.set_value();
            }
        };
    };
}

#endif