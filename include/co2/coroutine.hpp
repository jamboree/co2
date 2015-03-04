/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_COROUTINE_HPP_INCLUDED
#define CO2_COROUTINE_HPP_INCLUDED

#include <atomic>
#include <utility>
#include <exception>
#include <boost/assert.hpp>
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
    namespace temp
    {
        static constexpr std::size_t bytes = sizeof(void*) * 4;
        using storage = std::aligned_storage_t<bytes, alignof(std::max_align_t)>;

        template<class T, bool fit>
        struct traits_non_ref
        {
            static void create(void* p, T&& t)
            {
                new(p) T(std::move(t));
            }

            static T& get(void* p)
            {
                return *static_cast<T*>(p);
            }

            static void reset(void* p)
            {
                static_cast<T*>(p)->~T();
            }
        };

        template<class T>
        struct traits_non_ref<T, false>
        {
            static void create(void* p, T&& t)
            {
                *static_cast<T**>(p) = new T(std::move(t));
            }

            static T& get(void* p)
            {
                return **static_cast<T**>(p);
            }

            static void reset(void* p)
            {
                delete *static_cast<T**>(p);
            }
        };

        template<class T>
        struct traits : traits_non_ref<T, !(sizeof(T) > bytes) > {};

        template<class T>
        struct traits<T&>
        {
            static void create(void* p, T& t)
            {
                *static_cast<T**>(p) = &t;
            }

            static T& get(void* p)
            {
                return **static_cast<T**>(p);
            }

            static void reset(void* p) {}
        };

        template<class T>
        struct auto_reset
        {
            void* tmp;

            ~auto_reset()
            {
                traits<T>::reset(tmp);
            }
        };
    }

    struct resumable_base
    {
        temp::storage _tmp;
        std::atomic<unsigned> _use_count {1};
        unsigned _next;
        unsigned _eh;
        virtual void run(coroutine<> const&) noexcept = 0;
        virtual void release(coroutine<> const&) noexcept = 0;
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
        frame(Pack&& pack) : f(std::forward<Pack>(pack))
        {
            _next = F::_co2_start::value;
            _eh = F::_co2_stop::value;
        }

        void run(coroutine<> const& coro) noexcept override
        {
            f(static_cast<coroutine<Promise> const&>(coro), _next, _eh, &_tmp);
        }

        void release(coroutine<> const& coro) noexcept override
        {
            f(static_cast<coroutine<Promise> const&>(coro), ++_next, _eh, &_tmp);
            delete this;
        }
    };
    
    template<class R>
    using promise_t = typename R::promise_type;

    struct avoid_plain_return
    {
        explicit avoid_plain_return() = default;
    };

    template<class Promise>
    inline auto final_result(Promise* p) -> decltype(p->set_result())
    {
        p->set_result();
    }

    inline void final_result(void*)
    {
        BOOST_ASSERT_MSG(false, "missing return statement");
    }
}}

namespace co2
{
    template<>
    struct coroutine<void>
    {
        struct promise_type;

        coroutine() noexcept : _ptr() {}

        coroutine(std::nullptr_t) noexcept : _ptr() {}

        explicit coroutine(detail::resumable_base* ptr) noexcept : _ptr(ptr) {}

        coroutine(coroutine const& other) : _ptr(other._ptr)
        {
            _ptr->_use_count.fetch_add(1, std::memory_order_relaxed);
        }
        
        coroutine(coroutine&& other) noexcept : _ptr(other._ptr)
        {
            other._ptr = nullptr;
        }

        coroutine& operator=(coroutine other) noexcept
        {
            std::swap(_ptr, other._ptr);
            return *this;
        }

        ~coroutine()
        {
            if (_ptr && _ptr->_use_count.fetch_sub(1, std::memory_order_relaxed) == 1)
                _ptr->release(*this);
        }

        void swap(coroutine& other) noexcept
        {
            std::swap(_ptr, other._ptr);
        }

        explicit operator bool() const noexcept
        {
            return !!_ptr;
        }

        void operator()() const noexcept
        {
            _ptr->run(*this);
        }

    protected:

        detail::resumable_base* _ptr;
    };

    template<class Promise>
    struct coroutine : coroutine<>
    {
        using promise_type = Promise;

        coroutine() = default;

        coroutine(std::nullptr_t) noexcept {}

        explicit coroutine(detail::resumable<Promise>* p) noexcept : coroutine<>(p) {}

        explicit coroutine(Promise* p) noexcept
        {
            if (p)
            {
                _ptr = detail::resumable<Promise>::from_promise(p);
                _ptr->_use_count.fetch_add(1, std::memory_order_relaxed);
            }
        }

        Promise& promise() const noexcept
        {
            return static_cast<detail::resumable<Promise>*>(_ptr)->promise();
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

#define _impl_CO2_AWAIT(ret, var, next)                                         \
{                                                                               \
    using _co2_await = co2::detail::temp::traits<decltype(var)>;                \
    _co2_await::create(_co2_tmp, var);                                          \
    if (!_co2_await::get(_co2_tmp).await_ready())                               \
    {                                                                           \
        _co2_next = next;                                                       \
        _co2_await::get(_co2_tmp).await_suspend(_co2_coro);                     \
        return co2::detail::avoid_plain_return{};                               \
    }                                                                           \
    case next:                                                                  \
    if (_co2_promise.cancellation_requested())                                  \
    {                                                                           \
        case __COUNTER__:                                                       \
        _co2_await::reset(_co2_tmp);                                            \
        return co2::detail::avoid_plain_return{};                               \
    }                                                                           \
    co2::detail::temp::auto_reset<decltype(var)> _co2_reset = {_co2_tmp};       \
    ret (_co2_await::get(_co2_tmp).await_resume());                             \
}                                                                               \
/***/

#define CO2_AWAIT_GET(ret, var) _impl_CO2_AWAIT(ret =, var, __COUNTER__)
#define CO2_AWAIT(expr) _impl_CO2_AWAIT(, expr, __COUNTER__)
#define CO2_AWAIT_LET(let, var, ...)                                            \
_impl_CO2_AWAIT(([this](let) __VA_ARGS__), var, __COUNTER__)
/***/

#define CO2_RETURN(...)                                                         \
{                                                                               \
    _co2_next = _co2_stop::value;                                               \
    _co2_promise.set_result(__VA_ARGS__);                                       \
    return co2::detail::avoid_plain_return{};                                   \
}                                                                               \
/***/

#define CO2_TRY                                                                 \
__pragma(warning(push))                                                         \
__pragma(warning(disable:4456))                                                 \
using _co2_prev_eh = _co2_curr_eh;                                              \
using _co2_curr_eh = std::integral_constant<unsigned, __COUNTER__>;             \
__pragma(warning(pop))                                                          \
_co2_eh = _co2_curr_eh::value;                                                  \
if (true)                                                                       \
/***/

#define CO2_CATCH                                                               \
else case _co2_curr_eh::value:                                                  \
    try                                                                         \
    {                                                                           \
        _co2_eh = _co2_prev_eh::value;                                          \
        std::rethrow_exception(_co2_ex);                                        \
    }                                                                           \
    catch                                                                       \
/***/

#define _impl_CO2_TYPE_MEMBER(r, _, e) using BOOST_PP_CAT(e, _CO2_t) = decltype(e);
#define _impl_CO2_AUTO_MEMBER(r, _, e) BOOST_PP_CAT(e, _CO2_t) e;
#define _impl_CO2_FWD_MEMBER(r, _, e) std::forward<decltype(e)>(e),

#define _impl_CO2_TUPLE_FOR_EACH_IMPL(macro, t)                                 \
BOOST_PP_SEQ_FOR_EACH(macro, ~, BOOST_PP_VARIADIC_TO_SEQ t)
/***/
#define _impl_CO2_TUPLE_FOR_EACH_EMPTY(macro, t)

#define _impl_CO2_TUPLE_FOR_EACH(macro, t)                                      \
    BOOST_PP_IF(BOOST_PP_IS_EMPTY t, _impl_CO2_TUPLE_FOR_EACH_EMPTY,            \
        _impl_CO2_TUPLE_FOR_EACH_IMPL)(macro, t)                                \
/***/

#define CO2_BEGIN(R, capture, ...)                                              \
{                                                                               \
    using _co2_P = co2::detail::promise_t<R>;                                   \
    using _co2_C = co2::coroutine<_co2_P>;                                      \
    _impl_CO2_TUPLE_FOR_EACH(_impl_CO2_TYPE_MEMBER, capture)                    \
    struct _co2_pack                                                            \
    {                                                                           \
        _impl_CO2_TUPLE_FOR_EACH(_impl_CO2_AUTO_MEMBER, capture)                \
    };                                                                          \
    _co2_pack pack = {_impl_CO2_TUPLE_FOR_EACH(_impl_CO2_FWD_MEMBER, capture)}; \
    struct _co2_op : _co2_pack                                                  \
    {                                                                           \
        __VA_ARGS__                                                             \
        _co2_op(_co2_pack&& pack) : _co2_pack(std::move(pack)) {}               \
        using _co2_start = std::integral_constant<unsigned, __COUNTER__>;       \
        co2::detail::avoid_plain_return operator()                              \
        (_co2_C const& _co2_coro, unsigned& _co2_next, unsigned& _co2_eh, void* _co2_tmp)\
        {                                                                       \
            auto& _co2_promise = _co2_coro.promise();                           \
            std::exception_ptr _co2_ex;                                         \
            _co2_try_again:                                                     \
            try                                                                 \
            {                                                                   \
                switch (_co2_next)                                              \
                {                                                               \
                case _co2_start::value:                                         \
                    CO2_AWAIT(_co2_promise.initial_suspend());                  \
                    using _co2_curr_eh = _co2_stop;                             \
/***/

#define CO2_END                                                                 \
                    _co2_next = _co2_stop::value;                               \
                    co2::detail::final_result(&_co2_promise);                   \
                    break;                                                      \
                case _co2_stop::value:                                          \
                    _co2_promise.set_exception(_co2_ex);                        \
                }                                                               \
            }                                                                   \
            catch (...)                                                         \
            {                                                                   \
                _co2_next = _co2_eh;                                            \
                _co2_ex = std::current_exception();                             \
                goto _co2_try_again;                                            \
            }                                                                   \
            return co2::detail::avoid_plain_return{};                           \
        }                                                                       \
        using _co2_stop = std::integral_constant<unsigned, __COUNTER__>;        \
    };                                                                          \
    _co2_C coro(new co2::detail::frame<_co2_P, _co2_op>(std::move(pack)));      \
    coro();                                                                     \
    return coro.promise().get_return_object();                                  \
}                                                                               \
/***/

#endif