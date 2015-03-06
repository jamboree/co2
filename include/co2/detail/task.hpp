/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_DETAIL_TASK_HPP_INCLUDED
#define CO2_DETAIL_TASK_HPP_INCLUDED

#include <atomic>
#include <co2/coroutine.hpp>
#include <co2/detail/storage.hpp>

namespace co2 { namespace task_detail
{
    enum class tag
    {
        null, value, exception
    };

    struct promise_base
    {
        suspend_never initial_suspend()
        {
            return{};
        }

        suspend_never final_suspend()
        {
            return{};
        }

        bool cancellation_requested() const
        {
            return false;
        }
    };
    
    template<class T, class Base>
    struct promise : Base
    {
        using val_t = typename detail::wrap_reference<T>::type;

        template<class U>
        void set_result(U&& u)
        {
            new(&_data.value) val_t(std::forward<U>(u));
            Base::_tag.store(tag::value, std::memory_order_release);
            Base::notify();
        }

        void set_exception(std::exception_ptr const& e)
        {
            new(&_data.exception) std::exception_ptr(e);
            Base::_tag.store(tag::exception, std::memory_order_release);
            Base::notify();
        }

        T&& get()
        {
            if (Base::_tag.load(std::memory_order_acquire) == tag::exception)
                std::rethrow_exception(_data.exception);
            return static_cast<T&&>(_data.value);
        }

        ~promise()
        {
            switch (Base::_tag.load(std::memory_order_relaxed))
            {
            case tag::value:
                _data.value.~val_t();
                break;
            case tag::exception:
                _data.exception.~exception_ptr();
            default:
                break;
            }
        }

        detail::storage<val_t> _data;
    };

    template<class Base>
    struct promise<void, Base> : Base
    {
        void set_result()
        {
            Base::_tag.store(tag::value, std::memory_order_release);
            Base::notify();
        }

        void set_exception(std::exception_ptr const& e)
        {
            _e = e;
            Base::_tag.store(tag::exception, std::memory_order_release);
            Base::notify();
        }

        void get()
        {
            if (Base::_tag.load(std::memory_order_acquire) == tag::exception)
                std::rethrow_exception(_e);
        }

        std::exception_ptr _e;
    };
    
    template<class Derived, class T, class V, class Promise>
    struct impl
    {
        struct promise_type
          : promise<T, Promise>
        {
            Derived get_return_object()
            {
                return Derived(*this);
            }
        };

        impl() = default;

        explicit impl(promise_type& p) : _coro(&p) {}

        explicit operator bool() const noexcept
        {
            return static_cast<bool>(_coro);
        }

        bool valid() const noexcept
        {
            return static_cast<bool>(_coro);
        }

        void swap(impl& other) noexcept
        {
            _coro.swap(other._coro);
        }

        void reset() noexcept
        {
            _coro.reset();
        }

        bool await_ready() const noexcept
        {
            return _coro.promise()._tag.load(std::memory_order_relaxed) != task_detail::tag::null;
        }

        bool await_suspend(coroutine<> const& cb)
        {
            return _coro.promise().follow(cb);
        }

        V await_resume()
        {
            return _coro.promise().get();
        }

    private:

        coroutine<promise_type> _coro;
    };
}}

#endif
