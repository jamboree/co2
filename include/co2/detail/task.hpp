/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_DETAIL_TASK_HPP_INCLUDED
#define CO2_DETAIL_TASK_HPP_INCLUDED

#include <atomic>
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
            Base::_tag.store(tag::value, std::memory_order_release);
        }

        void set_exception(std::exception_ptr e) noexcept
        {
            new(&_data.exception) std::exception_ptr(std::move(e));
            Base::_tag.store(tag::exception, std::memory_order_release);
        }

        T&& get()
        {
            switch (Base::_tag.load(std::memory_order_acquire))
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
            _data.destroy(Base::_tag.load(std::memory_order_relaxed));
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
            switch (Base::_tag.load(std::memory_order_acquire))
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
                Promise::_tag.store(tag::cancelled, std::memory_order_release);
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

        bool is_cancelled() const noexcept
        {
            return _promise->_tag.load(std::memory_order_relaxed) == tag::cancelled;
        }

        bool await_ready() const noexcept
        {
            return !_promise->_then.load(std::memory_order_relaxed);
        }

        bool await_suspend(coroutine<>& cb) noexcept
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