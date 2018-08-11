/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2018 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_DETAIL_TASK_HPP_INCLUDED
#define CO2_DETAIL_TASK_HPP_INCLUDED

#include <atomic>
#include <type_traits>
#include <co2/coroutine.hpp>
#include <co2/utility/task_cancelled.hpp>
#include <co2/detail/storage.hpp>

namespace co2
{
    template<class T = void>
    struct task;

    template<class T = void>
    struct shared_task;
}

namespace co2 { namespace task_detail
{
    using detail::tag;

    template<class T>
    using cref_t = std::add_lvalue_reference_t<std::add_const_t<T>>;

    struct promise_base
    {
        bool initial_suspend() noexcept
        {
            return false;
        }

        bool cancellation_requested() const noexcept
        {
            return false;
        }
    };
    
    template<class T, class Base>
    struct promise_data : Base
    {
        using val_t = detail::wrap_reference_t<T>;

        template<class U = T>
        void set_result(U&& u)
        {
            new(&_data.value) val_t(std::forward<U>(u));
            Base::_tag = tag::value;
        }

        void set_exception(std::exception_ptr e) noexcept
        {
            new(&_data.exception) std::exception_ptr(std::move(e));
            Base::_tag = tag::exception;
        }

        T&& get()
        {
            switch (Base::_tag)
            {
            case tag::exception:
                std::rethrow_exception(_data.exception);
            case tag::cancelled:
                throw task_cancelled();
            default:
                break;
            }
            return static_cast<T&&>(_data.value);
        }

        ~promise_data()
        {
            _data.destroy(Base::_tag);
        }

        detail::storage<val_t> _data;
    };

    template<class Base>
    struct promise_data<void, Base> : Base
    {
        void set_result() noexcept
        {
            Base::_tag = tag::value;
        }

        void set_exception(std::exception_ptr e) noexcept
        {
            _e = std::move(e);
            Base::_tag = tag::exception;
        }

        void get()
        {
            switch (Base::_tag)
            {
            case tag::exception:
                std::rethrow_exception(_e);
            case tag::cancelled:
                throw task_cancelled();
            default:
                return;
            }
        }

        std::exception_ptr _e;
    };

    template<class Derived, class Promise>
    struct impl;
    
    template<template<class> class Task, class T, class Promise>
    struct impl<Task<T>, Promise>
    {
        struct promise_type : promise_data<T, Promise>
        {
            Task<T> get_return_object(coroutine<promise_type>& coro)
            {
                coro();
                return Task<T>(this);
            }

            void cancel() noexcept
            {
                Promise::_tag = tag::cancelled;
            }
        };

        impl() noexcept : _promise() {}

        impl(impl&& other) noexcept : _promise(other._promise)
        {
            other._promise = nullptr;
        }

        impl& operator=(impl&& other) noexcept
        {
            if (_promise)
                release();
            _promise = other._promise;
            other._promise = nullptr;
            return *this;
        }

        explicit impl(promise_type* promise) noexcept : _promise(promise) {}

        ~impl()
        {
            if (_promise)
                release();
        }

        explicit operator bool() const noexcept
        {
            return !!_promise;
        }

        bool valid() const noexcept
        {
            return !!_promise;
        }

        void swap(Task<T>& other) noexcept
        {
            std::swap(_promise, other._promise);
        }

        void reset() noexcept
        {
            if (_promise)
            {
                release();
                _promise = nullptr;
            }
        }

        bool is_cancelled() const noexcept
        {
            return _promise->_tag == tag::cancelled;
        }

        bool await_ready() const noexcept
        {
            return !_promise->_then.load(std::memory_order_acquire);
        }

        bool await_suspend(coroutine<>& cb) noexcept
        {
            return _promise->follow(cb);
        }

    protected:
        void release() noexcept
        {
            if (_promise->test_last())
                coroutine<promise_type>::destroy(_promise);
        }

        promise_type* _promise;
    };

    template<class ToTask, class FromTask>
    auto convert(FromTask t) CO2_BEG(ToTask, (t), CO2_TEMP_SIZE(0);)
    {
        if (!t.await_ready())
        {
            CO2_SUSPEND([&](coroutine<>& coro)
            {
                return t.await_suspend(coro);
            });
        }
        CO2_RETURN(t.await_resume());
    } CO2_END
}}

#endif