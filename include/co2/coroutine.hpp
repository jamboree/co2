/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_COROUTINE_HPP_INCLUDED
#define CO2_COROUTINE_HPP_INCLUDED

#include <new>
#include <memory>
#include <utility>
#include <exception>
#include <boost/config.hpp>
#include <boost/assert.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>
#include <boost/preprocessor/arithmetic/mod.hpp>
#include <boost/preprocessor/arithmetic/dec.hpp>
#include <boost/preprocessor/punctuation/remove_parens.hpp>
#include <co2/detail/void.hpp>

namespace co2
{
    template<class Promise = void>
    struct coroutine;

    template<class R>
    struct coroutine_traits
    {
        using promise_type = typename R::promise_type;
    };

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

    namespace detail
    {
        struct resumable_base
        {
            unsigned _next;
            unsigned _eh;
            void* _data;
            virtual void run(coroutine<>&) = 0;
            virtual void unwind(coroutine<>&) = 0;
            virtual void release() noexcept = 0;
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

        struct trivial_promise_base
        {
            bool initial_suspend() noexcept
            {
                return false;
            }

            bool final_suspend() noexcept
            {
                return false;
            }

            bool cancellation_requested() const noexcept
            {
                return false;
            }

            void set_result() noexcept {}
        };
    }

    using coroutine_handle = detail::resumable_base*;

    inline void*& coroutine_data(coroutine_handle h)
    {
        return h->_data;
    }

    template<>
    struct coroutine<void>
    {
        using handle_type = coroutine_handle;

        struct promise_type;

        coroutine() noexcept : _ptr() {}

        explicit coroutine(handle_type handle) noexcept : _ptr(handle) {}

        coroutine(coroutine&& other) noexcept : _ptr(other._ptr)
        {
            other._ptr = nullptr;
        }

        coroutine& operator=(coroutine other) noexcept
        {
            this->~coroutine();
            return *new(this) coroutine(std::move(other));
        }

        ~coroutine()
        {
            if (_ptr)
                _ptr->unwind(*this);
        }

        void reset() noexcept
        {
            if (_ptr)
            {
                _ptr->unwind(*this);
                _ptr = nullptr;
            }
        }

        void reset(handle_type handle) noexcept
        {
            if (_ptr)
                _ptr->unwind(*this);
            _ptr = handle;
        }

        void swap(coroutine& other) noexcept
        {
            std::swap(_ptr, other._ptr);
        }

        explicit operator bool() const noexcept
        {
            return !!_ptr;
        }

        void operator()() noexcept
        {
            _ptr->run(*this);
        }

        void resume()
        {
            _ptr->run(*this);
        }

        void* to_address() const noexcept
        {
            return _ptr;
        }

        handle_type handle() const noexcept
        {
            return _ptr;
        }

        handle_type detach() noexcept
        {
            auto handle = _ptr;
            _ptr = nullptr;
            return handle;
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
                _ptr = detail::resumable<Promise>::from_promise(p);
        }

        Promise& promise() const noexcept
        {
            return static_cast<detail::resumable<Promise>*>(_ptr)->promise();
        }

        static void destroy(Promise* p)
        {
            detail::resumable<Promise>::from_promise(p)->release();
        }

        static handle_type from_promise(Promise* p)
        {
            return detail::resumable<Promise>::from_promise(p);
        }
    };

    struct coroutine<>::promise_type : detail::trivial_promise_base
    {
        coroutine<promise_type> get_return_object(coroutine<promise_type>& coro)
        {
            coro.resume();
            return std::move(coro);
        }
    };

    template<>
    struct coroutine_traits<void>
    {
        struct promise_type : detail::trivial_promise_base
        {
            void get_return_object(coroutine<promise_type>& coro)
            {
                coro();
            }

            void set_exception(std::exception_ptr const&) noexcept {}
        };
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

    namespace detail
    {
        template<class F>
        void coroutine_local_sched(coroutine_handle then, F f) noexcept
        {
            thread_local coroutine_handle* chain = nullptr;
            if (chain)
            {
                auto& next = *chain;
                coroutine_data(then) = next;
                next = then;
            }
            else
            {
                chain = &then;
                {
                    coroutine<> coro{then};
                    then = nullptr;
                    f(coro);
                }
                while (then)
                {
                    coroutine<> coro{then};
                    then = static_cast<coroutine_handle>(coroutine_data(then));
                    f(coro);
                }
                chain = nullptr;
            }
        }
    }

    inline void coroutine_final_run(coroutine_handle then) noexcept
    {
        detail::coroutine_local_sched(then, [](coroutine<>& coro) { coro(); });
    }

    inline void coroutine_final_cancel(coroutine_handle then) noexcept
    {
        detail::coroutine_local_sched(then, [](coroutine<>&) {/*noop*/});
    }
}

namespace co2 { namespace detail
{
    namespace temp
    {
        struct warning;

        template<std::size_t Bytes, std::size_t RefSize = 0>
        using adjust_size = std::integral_constant<std::size_t, (Bytes > RefSize ? Bytes : RefSize)>;

        struct default_size
        {
            using _co2_sz = std::integral_constant<std::size_t, sizeof(void*) * 4>;
        };

        template<class T, bool NeedsAlloc>
        struct traits_non_ref
        {
            static void create(void* p, T&& t)
            {
#   if defined(CO2_WARN_DYN_ALLOC)
#       if defined(BOOST_MSVC)
                warning resort_to_dynamic_allocation(char(&)[sizeof(T)]);
#       else
                warning* resort_to_dynamic_allocation;
#       endif
#   endif
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
        struct traits_non_ref<T, false>
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
        struct traits_ref
        {
            static void create(void* p, T& t)
            {
                *static_cast<T**>(p) = &t;
            }

            static T& get(void* p)
            {
                return **static_cast<T**>(p);
            }

            static void reset(void*) {}
        };

        template<class T, std::size_t Bytes>
        struct traits_non_ref_check
        {
            static constexpr bool needs_alloc = sizeof(T) > Bytes;

            static_assert(!needs_alloc || Bytes >= sizeof(void*),
                "CO2_TEMP_SIZE too small to hold a pointer");

            using type = traits_non_ref<T, needs_alloc>;
        };

        template<class T, std::size_t Bytes>
        struct traits : traits_non_ref_check<T, Bytes>::type {};

        template<class T, std::size_t Bytes>
        struct traits<T&, Bytes> : traits_ref<T>
        {
            static_assert(Bytes >= sizeof(void*),
                "CO2_TEMP_SIZE too small to hold a reference");
        };

        template<class T, std::size_t Bytes>
        struct auto_reset
        {
            void* tmp;

            ~auto_reset()
            {
                traits<T, Bytes>::reset(tmp);
            }
        };
    }

    template<class T>
    using storage_for = std::aligned_storage_t<sizeof(T), alignof(T)>;

    struct exception_storage
    {
        void set(std::exception_ptr&& e) noexcept
        {
            new(&_data) std::exception_ptr(std::move(e));
        }

        std::exception_ptr get() noexcept
        {
            auto& ex = *reinterpret_cast<std::exception_ptr*>(&_data);
            std::exception_ptr ret(std::move(ex));
            ex.~exception_ptr();
            return ret;
        }

        storage_for<std::exception_ptr> _data;
    };

    using sentinel = std::integral_constant<unsigned, ~1u>;

    template<class Promise>
    inline auto try_suspend(Promise* p) -> decltype(p->try_suspend())
    {
        { decltype(p->try_resume()) is_also_required(void); }
        { decltype(p->try_cancel()) is_also_required(void); }
        return p->try_suspend();
    }

    inline std::true_type try_suspend(void*) noexcept
    {
        return {};
    }

    template<class Promise>
    inline auto try_resume(Promise* p) -> decltype(p->try_resume())
    {
        { decltype(p->try_suspend()) is_also_required(void); }
        { decltype(p->try_cancel()) is_also_required(void); }
        return p->try_resume();
    }

    inline std::true_type try_resume(void*) noexcept
    {
        return {};
    }

    template<class Promise>
    inline auto try_cancel(Promise* p) -> decltype(p->try_cancel())
    {
        { decltype(p->try_suspend()) is_also_required(void); }
        { decltype(p->try_resume()) is_also_required(void); }
        return p->try_cancel();
    }

    inline std::true_type try_cancel(void*) noexcept
    {
        return {};
    }

    template<class Alloc, class T>
    using rebind_alloc_t =
        typename std::allocator_traits<Alloc>::template rebind_alloc<T>;

    template<class F, class Alloc, std::size_t N>
    struct frame_storage : Alloc
    {
        explicit frame_storage(Alloc&& alloc) : Alloc(std::move(alloc)) {}

        alignas(std::max_align_t) char tmp[N];
        storage_for<F> f;
    };

    template<class F, class Alloc>
    struct frame_storage<F, Alloc, 0> : Alloc
    {
        explicit frame_storage(Alloc&& alloc) : Alloc(std::move(alloc)) {}

        union
        {
            storage_for<F> f;
            char tmp; // dummy
        };
    };

    template<class Promise, class F, class Alloc>
    struct frame final
      : resumable<Promise>
    {
        using alloc_t = rebind_alloc_t<Alloc, frame<Promise, F, Alloc>>;
        using traits = std::allocator_traits<alloc_t>;

        frame_storage<F, alloc_t, F::_co2_sz::value> _mem;

        template<class Pack>
        static frame* create(alloc_t alloc, Pack&& pack)
        {
            auto p = traits::allocate(alloc, 1);
            try
            {
                return new(p) frame(alloc, std::forward<Pack>(pack));
            }
            catch (...)
            {
                traits::deallocate(alloc, p, 1);
                throw;
            }
        }

        template<class Pack>
        frame(alloc_t alloc, Pack&& pack) : _mem(std::move(alloc))
        {
            new(&_mem.f) F(std::forward<Pack>(pack));
            this->_next = F::_co2_start::value;
        }

        void run(coroutine<>& coro) override
        {
            if (detail::try_resume(&this->promise()))
            {
                reinterpret_cast<F&>(_mem.f)
                (static_cast<coroutine<Promise>&>(coro), this->_next, this->_eh, &_mem.tmp);
            }
            else
                coro.detach();
        }

        void unwind(coroutine<>& coro) override
        {
            if (detail::try_cancel(&this->promise()))
            {
                reinterpret_cast<F&>(_mem.f)
                (static_cast<coroutine<Promise>&>(coro), ++this->_next, this->_eh, &_mem.tmp);
            }
        }

        void release() noexcept override
        {
            alloc_t alloc(static_cast<alloc_t&&>(_mem));
            this->~frame();
            traits::deallocate(alloc, this, 1);
        }
    };

    template<class Promise, class Locals, class Alloc, std::size_t Tmp>
    struct frame_size_helper : resumable<Promise>
    {
        frame_storage<Locals, Alloc, Tmp> _mem;
    };

    template<class Promise, class Locals, class Alloc, std::size_t Tmp>
    constexpr std::size_t frame_size =
        sizeof(frame_size_helper<Promise, Locals, Alloc, Tmp>);

    template<class T>
    using promise_t = typename T::promise_type;

    template<class T>
    T unrvref(T&&);

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

    template<class Promise>
    inline auto set_exception(Promise* p, exception_storage& ex) -> decltype(p->set_exception(ex.get()))
    {
        return p->set_exception(ex.get());
    }

    inline void set_exception(void*, exception_storage& ex)
    {
        std::rethrow_exception(ex.get());
    }

    template<class Promise>
    inline auto cancel(Promise* p) -> decltype(p->cancel())
    {
        return p->cancel();
    }

    inline void cancel(void*) {}

    template<class... T>
    inline std::allocator<void> get_alloc(T&&...)
    {
        return {};
    }

    template<class A, class... T>
    inline A get_alloc(std::allocator_arg_t, A a, T&&...)
    {
        return a;
    }

    template<class F, class P>
    struct finalizer
    {
        F* f;
        coroutine<P>& coro;
        P& promise;

        ~finalizer()
        {
            f->~F();
            coro.detach(); // this order is important!
            if (!promise.final_suspend())
                coroutine<P>::destroy(&promise);
        }
    };

    template<class T>
    inline auto await_ready(T& t) -> decltype(t.await_ready())
    {
        return t.await_ready();
    }

    template<class T, class F>
    inline auto await_suspend(T& t, F&& f) ->
        decltype(t.await_suspend(std::forward<F>(f)))
    {
        return t.await_suspend(std::forward<F>(f));
    }

    template<class T>
    inline auto await_resume(T& t) -> decltype(t.await_resume())
    {
        return t.await_resume();
    }

    struct await_ready_fn
    {
        template<class T>
        auto operator()(T& t) const -> decltype(await_ready(t))
        {
            return await_ready(t);
        }
    };

    struct await_suspend_fn
    {
        template<class T, class F>
        auto operator()(T& t, F&& f) const ->
            decltype(await_suspend(t, std::forward<F>(f)))
        {
            return await_suspend(t, std::forward<F>(f));
        }
    };

    struct await_resume_fn
    {
        template<class T>
        auto operator()(T& t) const -> decltype(await_resume(t))
        {
            return await_resume(t);
        }
    };
}}

namespace co2
{
    constexpr detail::await_ready_fn await_ready{};
    constexpr detail::await_suspend_fn await_suspend{};
    constexpr detail::await_resume_fn await_resume{};

    template<class T>
    struct await_result
    {
        using type = decltype(await_resume(std::declval<std::add_lvalue_reference_t<T>>()));
    };

    template<class T>
    using await_result_t = decltype(await_resume(std::declval<std::add_lvalue_reference_t<T>>()));

    namespace detail
    {
        template<class Task>
        struct ready_awaiter
        {
            Task task;

            bool await_ready()
            {
                return co2::await_ready(task);
            }

            template<class F>
            auto await_suspend(F&& f) -> decltype(co2::await_suspend(task, std::forward<F>(f)))
            {
                return co2::await_suspend(task, std::forward<F>(f));
            }

            void await_resume() noexcept {}
        };
    }

    template<class Task>
    inline detail::ready_awaiter<Task> ready(Task&& task)
    {
        return {std::forward<Task>(task)};
    }
}

#   if defined(BOOST_MSVC)
#   define Zz_CO2_IS_EMPTY BOOST_PP_IS_EMPTY
#   define Zz_CO2_PUSH_NAME_HIDDEN_WARNING                                      \
    __pragma(warning(push))                                                     \
    __pragma(warning(disable:4456))                                             \
    /***/
#   define Zz_CO2_POP_WARNING __pragma(warning(pop))
#   else
#   define Zz_CO2_PUSH_NAME_HIDDEN_WARNING
#   define Zz_CO2_POP_WARNING
// The IS_EMPTY trick is from:
// http://gustedt.wordpress.com/2010/06/08/detect-empty-macro-arguments/
// IS_EMPTY {
#   define Zz_CO2_ARG16(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, ...) _15
#   define Zz_CO2_HAS_COMMA(...) Zz_CO2_ARG16(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0)
#   define Zz_CO2_TRIGGER_PARENTHESIS_(...) ,

#   define Zz_CO2_IS_EMPTY(...)                                                 \
    Zz_CO2_IS_EMPTY_IMPL(                                                       \
        /* test if there is just one argument, eventually an empty one */       \
        Zz_CO2_HAS_COMMA(__VA_ARGS__),                                          \
        /* test if Zz_CO2_TRIGGER_PARENTHESIS_ together with the argument adds a comma */\
        Zz_CO2_HAS_COMMA(Zz_CO2_TRIGGER_PARENTHESIS_ __VA_ARGS__),              \
        /* test if the argument together with a parenthesis adds a comma */     \
        Zz_CO2_HAS_COMMA(__VA_ARGS__ (/*empty*/)),                              \
        /* test if placing it between Zz_CO2_TRIGGER_PARENTHESIS_ and the parenthesis adds a comma */\
        Zz_CO2_HAS_COMMA(Zz_CO2_TRIGGER_PARENTHESIS_ __VA_ARGS__ (/*empty*/))   \
    )                                                                           \
    /***/

#   define Zz_CO2_PASTE5(_0, _1, _2, _3, _4) _0 ## _1 ## _2 ## _3 ## _4
#   define Zz_CO2_IS_EMPTY_IMPL(_0, _1, _2, _3) Zz_CO2_HAS_COMMA(Zz_CO2_PASTE5(Zz_CO2_IS_EMPTY_CASE_, _0, _1, _2, _3))
#   define Zz_CO2_IS_EMPTY_CASE_0001 ,
    // } IS_EMPTY
#   endif

#define Zz_CO2_TUPLE_FOR_EACH_IMPL(macro, t)                                    \
BOOST_PP_SEQ_FOR_EACH(macro, ~, BOOST_PP_VARIADIC_TO_SEQ t)                     \
/***/

#define Zz_CO2_TUPLE_FOR_EACH_EMPTY(macro, t)

#define Zz_CO2_TUPLE_FOR_EACH(macro, t)                                         \
BOOST_PP_IF(Zz_CO2_IS_EMPTY t, Zz_CO2_TUPLE_FOR_EACH_EMPTY,                     \
    Zz_CO2_TUPLE_FOR_EACH_IMPL)(macro, t)                                       \
/***/

#   if defined(__has_cpp_attribute)
#       if __has_cpp_attribute(fallthrough)
#       define Zz_CO2_FALLTHROUGH [[fallthrough]];
#       endif
#   endif
#   if !defined(Zz_CO2_FALLTHROUGH)
#   define Zz_CO2_FALLTHROUGH
#   endif

// Try to detect statement expressions support.
#   if !defined(CO2_HAS_STMT_EXPR)
#       if defined(BOOST_GCC) | defined(BOOST_CLANG)
#       define CO2_HAS_STMT_EXPR
#       endif
#   endif

#   if defined(CO2_HAS_STMT_EXPR)
#   define Zz_CO2_STMT_EXPR_BEG (
#   define Zz_CO2_STMT_EXPR_END )
#   else
#   define Zz_CO2_STMT_EXPR_BEG do
#   define Zz_CO2_STMT_EXPR_END while (false)
#   endif

#define Zz_CO2_AWAIT(ret, expr, next)                                           \
Zz_CO2_STMT_EXPR_BEG {                                                          \
    using _co2_expr_t = decltype(::co2::detail::unrvref(expr));                 \
    using _co2_await = ::co2::detail::temp::traits<_co2_expr_t, _co2_sz::value>;\
    _co2_await::create(_co2_tmp, expr);                                         \
    try                                                                         \
    {                                                                           \
        if (!::co2::await_ready(_co2_await::get(_co2_tmp)))                     \
        {                                                                       \
            _co2_next = next;                                                   \
            if (!::co2::detail::try_suspend(&_co2_p))                           \
                goto BOOST_PP_CAT(_co2_cancel_, next);                          \
            if ((::co2::await_suspend(_co2_await::get(_co2_tmp), _co2_c),       \
                ::co2::detail::void_{}))                                        \
                return ::co2::detail::avoid_plain_return{};                     \
            else if (!::co2::detail::try_resume(&_co2_p))                       \
            {                                                                   \
                _co2_c.detach();                                                \
                return ::co2::detail::avoid_plain_return{};                     \
            }                                                                   \
        }                                                                       \
    }                                                                           \
    catch (...)                                                                 \
    {                                                                           \
        _co2_await::reset(_co2_tmp);                                            \
        throw;                                                                  \
    }                                                                           \
    Zz_CO2_FALLTHROUGH                                                          \
    case next:                                                                  \
    if (_co2_p.cancellation_requested())                                        \
    {                                                                           \
        case __COUNTER__:                                                       \
        BOOST_PP_CAT(_co2_cancel_, next):                                       \
        _co2_await::reset(_co2_tmp);                                            \
        ::co2::detail::cancel(&_co2_p);                                         \
        goto _co2_finalize;                                                     \
    }                                                                           \
    ::co2::detail::temp::auto_reset<_co2_expr_t, _co2_sz::value>                \
        _co2_reset = {_co2_tmp};                                                \
    ret (::co2::await_resume(_co2_await::get(_co2_tmp)));                       \
} Zz_CO2_STMT_EXPR_END                                                          \
/***/

#define Zz_CO2_SUSPEND(f, next)                                                 \
do {                                                                            \
    _co2_next = next;                                                           \
    if (!::co2::detail::try_suspend(&_co2_p))                                   \
        goto BOOST_PP_CAT(_co2_cancel_, next);                                  \
    if ((f(_co2_c), ::co2::detail::void_{}))                                    \
        return ::co2::detail::avoid_plain_return{};                             \
    else if (!::co2::detail::try_resume(&_co2_p))                               \
    {                                                                           \
        _co2_c.detach();                                                        \
        return ::co2::detail::avoid_plain_return{};                             \
    }                                                                           \
    Zz_CO2_FALLTHROUGH                                                          \
    case next:                                                                  \
    if (_co2_p.cancellation_requested())                                        \
    {                                                                           \
        case __COUNTER__:                                                       \
        BOOST_PP_CAT(_co2_cancel_, next):                                       \
        ::co2::detail::cancel(&_co2_p);                                         \
        goto _co2_finalize;                                                     \
    }                                                                           \
} while (false)                                                                 \
/***/

#define Zz_CO2_SUSPEND_IF(expr, next)                                           \
{                                                                               \
    if (_co2_p.expr)                                                            \
    {                                                                           \
        _co2_next = next;                                                       \
        if (!::co2::detail::try_suspend(&_co2_p))                               \
            goto BOOST_PP_CAT(_co2_cancel_, next);                              \
        return ::co2::detail::avoid_plain_return{};                             \
    }                                                                           \
    Zz_CO2_FALLTHROUGH                                                          \
    case next:                                                                  \
    if (_co2_p.cancellation_requested())                                        \
    {                                                                           \
        case __COUNTER__:                                                       \
        BOOST_PP_CAT(_co2_cancel_, next):                                       \
        ::co2::detail::cancel(&_co2_p);                                         \
        goto _co2_finalize;                                                     \
    }                                                                           \
}                                                                               \
/***/

#define CO2_AWAIT_APPLY(f, expr) Zz_CO2_AWAIT(f, expr, __COUNTER__)
#define CO2_AWAIT_SET(var, expr) Zz_CO2_AWAIT(var =, expr, __COUNTER__)
#define CO2_AWAIT(expr) Zz_CO2_AWAIT(, expr, __COUNTER__)
#define CO2_AWAIT_LET(let, expr, ...)                                           \
Zz_CO2_AWAIT(([this](let) __VA_ARGS__), expr, __COUNTER__)                      \
/***/

#define CO2_YIELD(...) CO2_AWAIT(_co2_p.yield_value(__VA_ARGS__))

#define CO2_SUSPEND(f) Zz_CO2_SUSPEND(f, __COUNTER__)

#define CO2_RETURN(...)                                                         \
do {                                                                            \
    _co2_next = ::co2::detail::sentinel::value;                                 \
    _co2_p.set_result(__VA_ARGS__);                                             \
    goto _co2_finalize;                                                         \
} while (false)                                                                 \
/***/

#define CO2_RETURN_LOCAL(var)                                                   \
do {                                                                            \
    _co2_next = ::co2::detail::sentinel::value;                                 \
    _co2_p.set_result(std::forward<decltype(var)>(var));                        \
    goto _co2_finalize;                                                         \
} while (false)                                                                 \
/***/

#define CO2_RETURN_FROM(...)                                                    \
do {                                                                            \
    _co2_next = ::co2::detail::sentinel::value;                                 \
    ::co2::detail::set_result(_co2_p, (__VA_ARGS__, ::co2::detail::void_{}));   \
    goto _co2_finalize;                                                         \
} while (false)                                                                 \
/***/

#define CO2_AWAIT_RETURN(expr) Zz_CO2_AWAIT(CO2_RETURN_FROM, expr, __COUNTER__)

#define CO2_TRY                                                                 \
Zz_CO2_PUSH_NAME_HIDDEN_WARNING                                                 \
using _co2_prev_eh = _co2_curr_eh;                                              \
using _co2_curr_eh = std::integral_constant<unsigned, __COUNTER__>;             \
Zz_CO2_POP_WARNING                                                              \
_co2_eh = _co2_curr_eh::value;                                                  \
if (true)                                                                       \
/***/

#define CO2_CATCH                                                               \
else case _co2_curr_eh::value:                                                  \
    try                                                                         \
    {                                                                           \
        _co2_eh = _co2_prev_eh::value;                                          \
        std::rethrow_exception(_co2_ex.get());                                  \
    }                                                                           \
    catch                                                                       \
/***/

#define Zz_CO2_SWITCH_LABEL(n) BOOST_PP_CAT(BOOST_PP_CAT(_co2_case_, __LINE__), n)

#define Zz_CO2_SWITCH_CASE(r, _, i, e)                                          \
BOOST_PP_IF(BOOST_PP_MOD(i, 2), , e: goto Zz_CO2_SWITCH_LABEL(i);)              \
/***/

#define Zz_CO2_UNPAREN(...) __VA_ARGS__
#define Zz_CO2_SWITCH_BODY_TRUE(i, e) Zz_CO2_SWITCH_LABEL(BOOST_PP_DEC(i)): Zz_CO2_UNPAREN e
#define Zz_CO2_SWITCH_BODY_FALSE(i, e)

#define Zz_CO2_SWITCH_BODY(r, _, i, e)                                          \
BOOST_PP_IF(BOOST_PP_MOD(i, 2),                                                 \
    Zz_CO2_SWITCH_BODY_TRUE, Zz_CO2_SWITCH_BODY_FALSE)(i, e)                    \
/***/

#define Zz_CO2_SWITCH(n, seq)                                                   \
switch (n)                                                                      \
{                                                                               \
    BOOST_PP_SEQ_FOR_EACH_I(Zz_CO2_SWITCH_CASE, ~, seq)                         \
}                                                                               \
while (false)                                                                   \
{                                                                               \
    BOOST_PP_SEQ_FOR_EACH_I(Zz_CO2_SWITCH_BODY, ~, seq)                         \
        break;                                                                  \
}                                                                               \
/***/

#define CO2_SWITCH(n, ...) Zz_CO2_SWITCH(n, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#if defined(BOOST_GCC)
#define Zz_CO2_TYPE_PARAM(r, _, e) using BOOST_PP_CAT(e, _t) = decltype(e);
#define Zz_CO2_DECL_PARAM(r, _, e) BOOST_PP_CAT(e, _t) e;
#define Zz_CO2_K(args)                                                          \
struct _co2_KK                                                                  \
{                                                                               \
    Zz_CO2_TUPLE_FOR_EACH(Zz_CO2_TYPE_PARAM, args)                              \
    struct pack                                                                 \
    {                                                                           \
        Zz_CO2_TUPLE_FOR_EACH(Zz_CO2_DECL_PARAM, args)                          \
    };                                                                          \
};                                                                              \
using _co2_K = typename _co2_KK::pack;                                          \
/***/
#else
#define Zz_CO2_DECL_PARAM(r, _, e) decltype(e) e;
#define Zz_CO2_K(args)                                                          \
struct _co2_K                                                                   \
{                                                                               \
    Zz_CO2_TUPLE_FOR_EACH(Zz_CO2_DECL_PARAM, args)                              \
};                                                                              \
/***/
#endif

#define Zz_CO2_FWD_PARAM(r, _, e) std::forward<decltype(e)>(e),
#define Zz_CO2_USE_PARAM(r, _, e) using _co2_K::e;

#define Zz_CO2_1ST(a, b) a
#define Zz_CO2_2ND(a, b) b

#define Zz_CO2_NEW_ALLOC(alloc, args) std::forward<decltype(alloc)>(alloc)
#define Zz_CO2_OLD_ALLOC(alloc, args)                                           \
::co2::detail::get_alloc(Zz_CO2_TUPLE_FOR_EACH(                                 \
    Zz_CO2_FWD_PARAM, args) ::co2::detail::void_{})                             \
/***/

#define Zz_CO2_INVOKE(f, args) f args
#define Zz_CO2_DISPATCHZz_CO2_GET_ALLOC_ (Zz_CO2_OLD_ALLOC, ~)
#define Zz_CO2_DISPATCH_NEW_ALLOC
#define Zz_CO2_GET_ALLOC_new(a) _NEW_ALLOC (Zz_CO2_NEW_ALLOC, a)
#define Zz_CO2_SKIP_CAPTURE(...)
#define Zz_CO2_GET_ALLOC(x) BOOST_PP_EXPAND(Zz_CO2_INVOKE(                      \
    BOOST_PP_CAT, (Zz_CO2_DISPATCH, Zz_CO2_INVOKE(                              \
        BOOST_PP_CAT, (Zz_CO2_GET_ALLOC_, Zz_CO2_SKIP_CAPTURE x)))))            \
/***/

#define Zz_CO2_SEPARATE_ALLOC(...) (__VA_ARGS__),
#define Zz_CO2_GET_ARGS(x) BOOST_PP_EXPAND(Zz_CO2_INVOKE(                       \
    Zz_CO2_1ST, (Zz_CO2_SEPARATE_ALLOC x)))                                     \
/***/

#define CO2_TEMP_SIZE(bytes) using _co2_sz = ::co2::detail::temp::adjust_size<bytes>

#   if defined(BOOST_MSVC) | defined(BOOST_CLANG)
namespace co2 { namespace detail
{
    template<class T, class R>
    R ret_of(R(T::*)());
}}
#       if defined(BOOST_MSVC)
#       define Zz_CO2_SUPPRESS_NO_DEF __pragma(warning(suppress:4822))
#       else
#       define Zz_CO2_SUPPRESS_NO_DEF
#       endif
#   define Zz_CO2_AUTO_F(var) BOOST_PP_CAT(_co2_auto_, var)
#   define CO2_AUTO(var, expr)                                                  \
    Zz_CO2_SUPPRESS_NO_DEF                                                      \
    auto Zz_CO2_AUTO_F(var)() -> std::decay_t<decltype(expr)>;                  \
    decltype(::co2::detail::ret_of(&_co2_F::Zz_CO2_AUTO_F(var))) var{expr}      \
    /***/
#   else
#   define CO2_AUTO(var, expr) std::decay_t<decltype(expr)> var{expr}
#   endif

#define Zz_CO2_HEAD(R, args, alloc, ...)                                        \
{                                                                               \
    using _co2_T = ::co2::coroutine_traits<BOOST_PP_REMOVE_PARENS(R)>;          \
    using _co2_P = ::co2::detail::promise_t<_co2_T>;                            \
    using _co2_C = ::co2::coroutine<_co2_P>;                                    \
    Zz_CO2_K(args)                                                              \
    _co2_K _co2_k = {Zz_CO2_TUPLE_FOR_EACH(Zz_CO2_FWD_PARAM, args)};            \
    auto _co2_a(Zz_CO2_1ST alloc(Zz_CO2_2ND alloc, args));                      \
    struct _co2_F : ::co2::detail::temp::default_size, _co2_K                   \
    {                                                                           \
        Zz_CO2_TUPLE_FOR_EACH(Zz_CO2_USE_PARAM, args)                           \
        __VA_ARGS__                                                             \
        _co2_F(_co2_K&& pack) : _co2_K(std::move(pack)) {}                      \
        using _co2_start = std::integral_constant<unsigned, __COUNTER__>;       \
        ::co2::detail::avoid_plain_return operator()                            \
        (_co2_C& _co2_c, unsigned& _co2_next, unsigned& _co2_eh, void* _co2_tmp)\
        {                                                                       \
            (void)_co2_tmp;                                                     \
            (void)_co2_sz::value;                                               \
            auto& _co2_p = _co2_c.promise();                                    \
            ::co2::detail::exception_storage _co2_ex;                           \
            _co2_try_again:                                                     \
            try                                                                 \
            {                                                                   \
                switch (_co2_next)                                              \
                {                                                               \
                case _co2_start::value:                                         \
                    using _co2_curr_eh = ::co2::detail::sentinel;               \
                    _co2_eh = _co2_curr_eh::value;                              \
                    Zz_CO2_SUSPEND_IF(initial_suspend(), __COUNTER__);          \
/***/

#define CO2_BEG(R, capture, ...) -> BOOST_PP_REMOVE_PARENS(R) Zz_CO2_HEAD(R,    \
    Zz_CO2_GET_ARGS(capture), Zz_CO2_GET_ALLOC(capture), __VA_ARGS__)           \
/***/

#define CO2_END                                                                 \
                    ::co2::detail::final_result(&_co2_p);                       \
                _co2_finalize:                                                  \
                    ::co2::detail::finalizer<_co2_F, _co2_P>{this, _co2_c, _co2_p};\
                }                                                               \
            }                                                                   \
            catch (...)                                                         \
            {                                                                   \
                _co2_next = _co2_eh;                                            \
                _co2_ex.set(std::current_exception());                          \
                if (_co2_next != ::co2::detail::sentinel::value)                \
                    goto _co2_try_again;                                        \
                ::co2::detail::finalizer<_co2_F, _co2_P> fin{this, _co2_c, _co2_p};\
                ::co2::detail::set_exception(&_co2_p, _co2_ex);                 \
            }                                                                   \
            return ::co2::detail::avoid_plain_return{};                         \
        }                                                                       \
    };                                                                          \
    using _co2_FR = ::co2::detail::frame<_co2_P, _co2_F, decltype(_co2_a)>;     \
    _co2_C _co2_c(_co2_FR::create(std::move(_co2_a), std::move(_co2_k)));       \
    return _co2_c.promise().get_return_object(_co2_c);                          \
}                                                                               \
/***/

#endif