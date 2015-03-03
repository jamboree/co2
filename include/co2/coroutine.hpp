/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_COROUTINE_HPP_INCLUDED
#define CO2_COROUTINE_HPP_INCLUDED

#include <utility>
#include <exception>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>

namespace co2
{
    template<class Promise = void>
    struct coroutine;
    
    template<bool flag>
    struct suspend
    {
        bool await_ready() noexcept
        {
            return !flag;
        }

        void await_suspend(coroutine<> const&) noexcept {}

        void await_resume() noexcept {}
    };

    using suspend_always = suspend<true>;
    using suspend_never = suspend<false>;
}

namespace co2 { namespace detail
{
    struct resumable_base : boost::intrusive_ref_counter<resumable_base>
    {
        unsigned _next = 0;
        virtual void run(coroutine<> const&) noexcept = 0;
        virtual ~resumable_base() {}
    };

    template<class Promise>
    struct resumable : resumable_base, private Promise
    {
        Promise& promise()
        {
            return *this;
        }

        static resumable* from_promise(Promise* p)
        {
            return static_cast<resumable*>(p);
        }
    };
    
    template<class Promise, class F>
    struct frame : resumable<Promise>
    {
        F f;

        template<class Pack>
        frame(Pack&& pack) : f(std::forward<Pack>(pack)) {}

        void run(coroutine<> const& coro) noexcept override
        {
            try
            {
                f(static_cast<coroutine<Promise> const&>(coro), _next);
            }
            catch (...)
            {
                promise().set_exception(std::current_exception());
            }
        }
    };
    
    template<class R>
    using promise_t = typename R::promise_type;
}}

namespace co2
{
    template<>
    struct coroutine<void>
    {
        struct promise_type;

        explicit coroutine(detail::resumable_base* p) : _ptr(p) {}

        explicit operator bool() const noexcept
        {
            return static_cast<bool>(_ptr);
        }

        void operator()() const noexcept
        {
            _ptr->run(*this);
        }

    protected:

        boost::intrusive_ptr<detail::resumable_base> _ptr;
    };

    template<class Promise>
    struct coroutine : coroutine<>
    {
        using promise_type = Promise;

        explicit coroutine(detail::resumable<Promise>* p) : coroutine<>(p) {}

        explicit coroutine(Promise* p)
            : coroutine<>(detail::resumable<Promise>::from_promise(p))
        {}

        Promise& promise() const noexcept
        {
            return static_cast<detail::resumable<Promise>*>(_ptr.get())->promise();
        }
    };

    struct coroutine<>::promise_type
    {
        suspend_never initial_suspend()
        {
            return {};
        }

        suspend_never final_suspend()
        {
            return {};
        }

        bool cancellation_requested() const
        {
            return false;
        }

        coroutine<promise_type> get_return_object()
        {
            return coroutine<promise_type>(this);
        }

        void set_result() {}

        void set_exception(std::exception_ptr const&)
        {
            std::terminate();
        }
    };
}

#define CO2_AWAIT(ret, var)                                                     \
if (!this->var.await_ready())                                                   \
{                                                                               \
    _co2_next = __LINE__;                                                       \
    this->var.await_suspend(_co2_coro);                                         \
    return;                                                                     \
}                                                                               \
case __LINE__:                                                                  \
if (_co2_promise.cancellation_requested())                                      \
    return;                                                                     \
ret = this->var.await_resume();                                                 \
/***/

#define CO2_AWAIT_VOID(expr)                                                    \
{                                                                               \
    auto&& var = expr;                                                          \
    if (!var.await_ready())                                                     \
    {                                                                           \
        _co2_next = __LINE__;                                                   \
        var.await_suspend(_co2_coro);                                           \
        return;                                                                 \
    }                                                                           \
}                                                                               \
case __LINE__:                                                                  \
if (_co2_promise.cancellation_requested())                                      \
    return;                                                                     \
/***/

#define CO2_LIST_MEMBER(r, _, e) e;
#define CO2_TYPE_MEMBER(r, _, e) using BOOST_PP_CAT(e, _CO2_t) = decltype(e);
#define CO2_AUTO_MEMBER(r, _, e) BOOST_PP_CAT(e, _CO2_t) e;
#define CO2_FWD_MEMBER(r, _, e) std::forward<decltype(e)>(e),

#define CO2_TUPLE_FOR_EACH_IMPL(macro, t)                                       \
BOOST_PP_SEQ_FOR_EACH(macro, ~, BOOST_PP_VARIADIC_TO_SEQ t)
/***/
#define CO2_TUPLE_FOR_EACH_EMPTY(macro, t)

#define CO2_TUPLE_FOR_EACH(macro, t)                                            \
    BOOST_PP_IF(BOOST_PP_IS_EMPTY t, CO2_TUPLE_FOR_EACH_EMPTY,                  \
        CO2_TUPLE_FOR_EACH_IMPL)(macro, t)                                      \
/***/

#define CO2_BEGIN(R, capture, ...)                                              \
{                                                                               \
    using _co2_P = co2::detail::promise_t<R>;                                   \
    using _co2_C = co2::coroutine<_co2_P>;                                      \
    CO2_TUPLE_FOR_EACH(CO2_TYPE_MEMBER, capture)                                \
    struct _co2_pack                                                            \
    {                                                                           \
        CO2_TUPLE_FOR_EACH(CO2_AUTO_MEMBER, capture)                            \
    };                                                                          \
    _co2_pack pack = {CO2_TUPLE_FOR_EACH(CO2_FWD_MEMBER, capture)};             \
    struct _co2_op : _co2_pack                                                  \
    {                                                                           \
        CO2_TUPLE_FOR_EACH(CO2_LIST_MEMBER, (__VA_ARGS__))                      \
        _co2_op(_co2_pack&& pack) : _co2_pack(std::move(pack)) {}               \
        void operator()(_co2_C const& _co2_coro, unsigned& _co2_next)           \
        {                                                                       \
            auto& _co2_promise = _co2_coro.promise();                           \
            switch (_co2_next)                                                  \
            {                                                                   \
            case 0:                                                             \
                CO2_AWAIT_VOID(_co2_promise.initial_suspend());                 \
/***/

#define CO2_END                                                                 \
            }                                                                   \
        }                                                                       \
    };                                                                          \
    _co2_C coro(new co2::detail::frame<_co2_P, _co2_op>(std::move(pack)));      \
    coro();                                                                     \
    return coro.promise().get_return_object();                                  \
}                                                                               \
/***/

#endif
