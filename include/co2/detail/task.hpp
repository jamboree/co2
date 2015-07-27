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

namespace co2
{
    template<class T = void>
    struct task;

    template<class T = void>
    struct shared_task;

    struct task_cancelled {};
}

namespace co2 { namespace task_detail
{
    using detail::tag;

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

        void set_result(T&& t)
        {
            set_result<T>(std::forward<T>(t));
        }

        template<class U>
        void set_result(U&& u)
        {
            new(&_data.value) val_t(std::forward<U>(u));
            Base::_tag.store(tag::value, std::memory_order_release);
        }

        void set_exception(std::exception_ptr e) noexcept
        {
            new(&_data.exception) std::exception_ptr(std::move(e));
            Base::_tag.store(tag::exception, std::memory_order_release);
        }

        T&& get()
        {
            if (Base::_tag.load(std::memory_order_acquire) == tag::exception)
                std::rethrow_exception(_data.exception);
            return static_cast<T&&>(_data.value);
        }

        ~promise_data()
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
    struct promise_data<void, Base> : Base
    {
        void set_result() noexcept
        {
            Base::_tag.store(tag::value, std::memory_order_release);
        }

        void set_exception(std::exception_ptr e) noexcept
        {
            _e = std::move(e);
            Base::_tag.store(tag::exception, std::memory_order_release);
        }

        void get()
        {
            if (Base::_tag.load(std::memory_order_acquire) == tag::exception)
                std::rethrow_exception(_e);
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
                this->set_exception(std::make_exception_ptr(task_cancelled{}));
            }

            bool final_suspend() noexcept
            {
                this->finalize();
                return !this->test_last();
            }
        };

        impl() noexcept : _promise() {}

        impl(impl&& other) noexcept : _promise(other._promise)
        {
            other._promise = nullptr;
        }

        impl& operator=(impl other) noexcept
        {
            this->~impl();
            return *new(this) impl(std::move(other));
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

        bool await_ready() const noexcept
        {
            return _promise->_tag.load(std::memory_order_relaxed) != tag::null;
        }

        bool await_suspend(coroutine<>& cb)
        {
            return _promise->follow(cb);
        }

    protected:

        void release()
        {
            if (_promise->test_last())
                coroutine<promise_type>::destroy(_promise);
        }

        promise_type* _promise;
    };

    template<class T>
    inline auto share(task<T> t) CO2_BEG(shared_task<T>, (t), CO2_TEMP_SIZE(sizeof(void*));)
    {
        CO2_AWAIT_RETURN(t);
    } CO2_END
}}

#endif