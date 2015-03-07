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
#include <boost/config.hpp>
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
        struct traits : traits_non_ref<T, !(sizeof(T) > bytes)> {};

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

    using sentinel = std::integral_constant<unsigned, ~1u>;

    struct resumable_base
    {
        temp::storage _tmp;
        std::atomic<unsigned> _use_count {1};
        unsigned _next;
        unsigned _eh;
        virtual void run(coroutine<> const&) noexcept = 0;
        virtual void release(coroutine<> const&) noexcept = 0;

        bool done() const noexcept
        {
            return _next == sentinel::value;
        }
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
    struct frame final : resumable<Promise>
    {
        F f;

        template<class Pack>
        frame(Pack&& pack) : f(std::forward<Pack>(pack))
        {
            this->_next = F::_co2_start::value;
        }

        void run(coroutine<> const& coro) noexcept override
        {
            f(static_cast<coroutine<Promise> const&>(coro), this->_next, this->_eh, &this->_tmp);
        }

        void release(coroutine<> const& coro) noexcept override
        {
            f(static_cast<coroutine<Promise> const&>(coro), ++this->_next, this->_eh, &this->_tmp);
            delete this;
        }
    };
    
    template<class R>
    using promise_t = typename R::promise_type;

    template<class T>
    T unrvref(T&&);

    struct avoid_plain_return
    {
        explicit avoid_plain_return() = default;
    };

    struct void_
    {
        operator bool() const noexcept
        {
            return true;
        }
    };

    template<class RHS>
    inline RHS&& operator,(RHS&& rhs, void_) noexcept
    {
        return static_cast<RHS&&>(rhs);
    }

    template<class Promise>
    inline auto final_result(Promise* p) -> decltype(p->set_result())
    {
        p->set_result();
    }

    inline void final_result(void*)
    {
        BOOST_ASSERT_MSG(false, "missing return statement");
    }

    template<class Promise, class T>
    inline void set_result(Promise& p, T&& t)
    {
        p.set_result(std::forward<T>(t));
    }

    template<class Promise>
    inline void set_result(Promise& p, void_)
    {
        p.set_result();
    }
}}

namespace co2
{
    template<>
    struct coroutine<void>
    {
        using handle_type = detail::resumable_base*;

        struct promise_type;

        coroutine() noexcept : _ptr() {}

        explicit coroutine(handle_type handle) noexcept : _ptr(handle) {}

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
            release_frame();
        }

        void reset() noexcept
        {
            release_frame();
            _ptr = nullptr;
        }

        void swap(coroutine& other) noexcept
        {
            std::swap(_ptr, other._ptr);
        }

        explicit operator bool() const noexcept
        {
            return !!_ptr;
        }

        bool done() const noexcept
        {
            return _ptr->done();
        }

        void operator()() const noexcept
        {
            _ptr->run(*this);
        }

        void* to_address() const noexcept
        {
            return _ptr;
        }

        handle_type release_handle() noexcept
        {
            auto handle = _ptr;
            _ptr = nullptr;
            return handle;
        }

    protected:

        void release_frame() noexcept
        {
            if (_ptr && _ptr->_use_count.fetch_sub(1, std::memory_order_relaxed) == 1)
                _ptr->release(*this);
        }

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

    template<class Promise>
    inline bool operator==(coroutine<Promise> const& lhs, coroutine<Promise> const& rhs)
    {
        return lhs.to_address() == rhs.to_address();
    }

    template<class Promise>
    inline bool operator!=(coroutine<Promise> const& lhs, coroutine<Promise> const& rhs)
    {
        return lhs.to_address() != rhs.to_address();
    }

    struct atomic_coroutine_handle
    {
        atomic_coroutine_handle() : _p{nullptr} {}

        atomic_coroutine_handle(atomic_coroutine_handle&&) = delete;

        coroutine<> exchange(coroutine<> coro)
        {
            return coroutine<>(_p.exchange(coro.release_handle(), std::memory_order_relaxed));
        }

        coroutine<> exchange_null()
        {
            return coroutine<>(_p.exchange(nullptr, std::memory_order_relaxed));
        }

        ~atomic_coroutine_handle()
        {
            coroutine<>(_p.load(std::memory_order_relaxed));
        }

        std::atomic<coroutine<>::handle_type> _p;
    };
}

#   if defined(BOOST_MSVC)
#   define _impl_CO2_IS_EMPTY BOOST_PP_IS_EMPTY
#   define _impl_CO2_PUSH_NAME_HIDDEN_WARNING                                   \
    __pragma(warning(push))                                                     \
    __pragma(warning(disable:4456))                                             \
    /***/
#   define _impl_CO2_POP_WARNING __pragma(warning(pop))
#   else
#   define _impl_CO2_PUSH_NAME_HIDDEN_WARNING
#   define _impl_CO2_POP_WARNING
    // The IS_EMPTY trick is from:
    // http://gustedt.wordpress.com/2010/06/08/detect-empty-macro-arguments/
    // IS_EMPTY {
#   define _impl_CO2__ARG16(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, ...) _15
#   define _impl_CO2_HAS_COMMA(...) _impl_CO2__ARG16(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0)
#   define _impl_CO2_TRIGGER_PARENTHESIS_(...) ,

#   define _impl_CO2_IS_EMPTY(...)                                              \
    _impl_CO2_IS_EMPTY_IMPL(                                                    \
        /* test if there is just one argument, eventually an empty one */       \
        _impl_CO2_HAS_COMMA(__VA_ARGS__),                                       \
        /* test if _impl_CO2_TRIGGER_PARENTHESIS_ together with the argument adds a comma */\
        _impl_CO2_HAS_COMMA(_impl_CO2_TRIGGER_PARENTHESIS_ __VA_ARGS__),        \
        /* test if the argument together with a parenthesis adds a comma */     \
        _impl_CO2_HAS_COMMA(__VA_ARGS__ (/*empty*/)),                           \
        /* test if placing it between _impl_CO2_TRIGGER_PARENTHESIS_ and the parenthesis adds a comma */\
        _impl_CO2_HAS_COMMA(_impl_CO2_TRIGGER_PARENTHESIS_ __VA_ARGS__ (/*empty*/))\
    )                                                                           \
    /***/

#   define _impl_CO2_PASTE5(_0, _1, _2, _3, _4) _0 ## _1 ## _2 ## _3 ## _4
#   define _impl_CO2_IS_EMPTY_IMPL(_0, _1, _2, _3) _impl_CO2_HAS_COMMA(_impl_CO2_PASTE5(_IS_EMPTY_CASE_, _0, _1, _2, _3))
#   define _impl_CO2_IS_EMPTY_CASE_0001 ,
    // } IS_EMPTY
#   endif

#define _impl_CO2_AWAIT(ret, expr, next)                                        \
{                                                                               \
    using _co2_expr_t = decltype(co2::detail::unrvref(expr));                   \
    using _co2_await = co2::detail::temp::traits<_co2_expr_t>;                  \
    _co2_await::create(_co2_tmp, expr);                                         \
    if (!_co2_await::get(_co2_tmp).await_ready())                               \
    {                                                                           \
        _co2_next = next;                                                       \
        if (_co2_await::get(_co2_tmp).await_suspend(_co2_coro),                 \
            co2::detail::void_{})                                               \
            return co2::detail::avoid_plain_return{};                           \
    }                                                                           \
    case next:                                                                  \
    if (_co2_promise.cancellation_requested())                                  \
    {                                                                           \
        case __COUNTER__:                                                       \
        _co2_await::reset(_co2_tmp);                                            \
        return co2::detail::avoid_plain_return{};                               \
    }                                                                           \
    co2::detail::temp::auto_reset<_co2_expr_t> _co2_reset = {_co2_tmp};         \
    ret (_co2_await::get(_co2_tmp).await_resume());                             \
}                                                                               \
/***/

#define CO2_AWAIT_SET(var, expr) _impl_CO2_AWAIT(var =, expr, __COUNTER__)
#define CO2_AWAIT(expr) _impl_CO2_AWAIT(, expr, __COUNTER__)
#define CO2_AWAIT_LET(let, expr, ...)                                           \
_impl_CO2_AWAIT(([this](let) __VA_ARGS__), expr, __COUNTER__)
/***/

#define CO2_YIELD(...) CO2_AWAIT(_co2_promise.yield_value(__VA_ARGS__))

#define CO2_RETURN(...)                                                         \
{                                                                               \
    _co2_next = co2::detail::sentinel::value;                                   \
    _co2_promise.set_result(__VA_ARGS__);                                       \
    return co2::detail::avoid_plain_return{};                                   \
}                                                                               \
/***/

#define CO2_RETURN_FROM(...)                                                    \
{                                                                               \
    _co2_next = co2::detail::sentinel::value;                                   \
    co2::detail::set_result(_co2_promise, (__VA_ARGS__, co2::detail::void_{})); \
    return co2::detail::avoid_plain_return{};                                   \
}                                                                               \
/***/

#define CO2_AWAIT_RETURN(expr) _impl_CO2_AWAIT(CO2_RETURN_FROM, expr, __COUNTER__)

#define CO2_TRY                                                                 \
_impl_CO2_PUSH_NAME_HIDDEN_WARNING                                              \
using _co2_prev_eh = _co2_curr_eh;                                              \
using _co2_curr_eh = std::integral_constant<unsigned, __COUNTER__>;             \
_impl_CO2_POP_WARNING                                                           \
_co2_eh = _co2_curr_eh::value;                                                  \
if (true)                                                                       \
/***/

#define CO2_CATCH                                                               \
else case _co2_curr_eh::value:                                                  \
    try                                                                         \
    {                                                                           \
        _co2_eh = _co2_prev_eh::value;                                          \
        std::rethrow_exception(std::move(_co2_ex));                             \
    }                                                                           \
    catch                                                                       \
/***/

#define _impl_CO2_DECL_MEMBER(r, _, e) decltype(e) e;
#define _impl_CO2_FWD_MEMBER(r, _, e) std::forward<decltype(e)>(e),
#define _impl_CO2_USE_MEMBER(r, _, e) using _co2_pack::e;

#define _impl_CO2_TUPLE_FOR_EACH_IMPL(macro, t)                                 \
BOOST_PP_SEQ_FOR_EACH(macro, ~, BOOST_PP_VARIADIC_TO_SEQ t)
/***/
#define _impl_CO2_TUPLE_FOR_EACH_EMPTY(macro, t)

#define _impl_CO2_TUPLE_FOR_EACH(macro, t)                                      \
    BOOST_PP_IF(_impl_CO2_IS_EMPTY t, _impl_CO2_TUPLE_FOR_EACH_EMPTY,           \
        _impl_CO2_TUPLE_FOR_EACH_IMPL)(macro, t)                                \
/***/

#define CO2_BEGIN(R, capture, ...)                                              \
{                                                                               \
    using _co2_P = co2::detail::promise_t<R>;                                   \
    using _co2_C = co2::coroutine<_co2_P>;                                      \
    struct _co2_pack                                                            \
    {                                                                           \
        _impl_CO2_TUPLE_FOR_EACH(_impl_CO2_DECL_MEMBER, capture)                \
    };                                                                          \
    _co2_pack pack = {_impl_CO2_TUPLE_FOR_EACH(_impl_CO2_FWD_MEMBER, capture)}; \
    struct _co2_op : _co2_pack                                                  \
    {                                                                           \
        _impl_CO2_TUPLE_FOR_EACH(_impl_CO2_USE_MEMBER, capture)                 \
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
                    using _co2_curr_eh = co2::detail::sentinel;                 \
                    _co2_eh = _co2_curr_eh::value;                              \
                    CO2_AWAIT(_co2_promise.initial_suspend());                  \
/***/

#define CO2_END                                                                 \
                    _co2_next = co2::detail::sentinel::value;                   \
                    co2::detail::final_result(&_co2_promise);                   \
                }                                                               \
            }                                                                   \
            catch (...)                                                         \
            {                                                                   \
                _co2_next = _co2_eh;                                            \
                _co2_ex = std::current_exception();                             \
                if (_co2_next != co2::detail::sentinel::value)                  \
                    goto _co2_try_again;                                        \
                _co2_promise.set_exception(std::move(_co2_ex));                 \
            }                                                                   \
            return co2::detail::avoid_plain_return{};                           \
        }                                                                       \
    };                                                                          \
    _co2_C coro(new co2::detail::frame<_co2_P, _co2_op>(std::move(pack)));      \
    coro();                                                                     \
    return coro.promise().get_return_object();                                  \
}                                                                               \
/***/

#define CO2_RET(R, capture, ...) -> R CO2_BEGIN(R, capture, __VA_ARGS__)

#endif