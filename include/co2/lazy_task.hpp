/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2016-2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_LAZY_TASK_HPP_INCLUDED
#define CO2_LAZY_TASK_HPP_INCLUDED

#include <co2/coroutine.hpp>
#include <co2/utility/task_cancelled.hpp>
#include <co2/detail/storage.hpp>
#include <co2/detail/final_reset.hpp>

namespace co2 { namespace detail
{
    struct lazy_promise_base
    {
        bool initial_suspend() noexcept
        {
            return false;
        }

        bool final_suspend() noexcept
        {
            if (_then)
            {
                coroutine_final_run(_then);
                return true;
            }
            return false;
        }

        bool cancellation_requested() const noexcept
        {
            return false;
        }

        coroutine_handle _then = nullptr;
    };

    template<class T>
    struct lazy_promise : lazy_promise_base
    {
        using val_t = wrap_reference_t<T>;

        template<class U = T>
        void set_result(U&& u)
        {
            new(&_data.value) val_t(std::forward<U>(u));
            _tag = tag::value;
        }

        void set_exception(std::exception_ptr e) noexcept
        {
            new(&_data.exception) std::exception_ptr(std::move(e));
            _tag = tag::exception;
        }

        T&& get()
        {
            switch (_tag)
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

        ~lazy_promise()
        {
            _data.destroy(_tag);
        }

        storage<val_t> _data;
        tag _tag = tag::null;
    };

    template<>
    struct lazy_promise<void> : lazy_promise_base
    {
        void set_result() noexcept
        {
            _tag = tag::value;
        }

        void set_exception(std::exception_ptr e) noexcept
        {
            _e = std::move(e);
            _tag = tag::exception;
        }

        void get()
        {
            switch (_tag)
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
        tag _tag = tag::null;
    };
}}

namespace co2
{
    template<class T = void>
    struct lazy_task
    {
        struct promise_type : detail::lazy_promise<T>
        {
            lazy_task get_return_object(coroutine<promise_type>& coro)
            {
                coro.detach();
                return lazy_task(this);
            }

            void cancel() noexcept
            {
                this->_tag = detail::tag::cancelled;
            }
        };

        lazy_task() noexcept : _promise() {}

        lazy_task(lazy_task&& other) noexcept : _promise(other._promise)
        {
            other._promise = nullptr;
        }

        explicit lazy_task(promise_type* promise) noexcept : _promise(promise) {}

        lazy_task& operator=(lazy_task other) noexcept
        {
            this->~lazy_task();
            return *new(this) lazy_task(std::move(other));
        }

        bool await_ready()
        {
            return _promise->_tag != detail::tag::null;
        }

        void await_suspend(coroutine<>& coro)
        {
            _promise->_then = coro.detach();
            coroutine_final_run(coroutine<promise_type>::from_promise(_promise));
        }

        T await_resume()
        {
            detail::final_reset<lazy_task> _{this};
            return _promise->get();
        }

        explicit operator bool() const noexcept
        {
            return !!_promise;
        }

        bool valid() const noexcept
        {
            return !!_promise;
        }

        void swap(lazy_task& other) noexcept
        {
            std::swap(_promise, other._promise);
        }

        void reset() noexcept
        {
            if (_promise)
            {
                if (_promise->_then)
                    coroutine<promise_type>::destroy(_promise);
                else
                    coroutine_final_run(coroutine<promise_type>::from_promise(_promise));
                _promise = nullptr;
            }
        }

        bool is_cancelled() const noexcept
        {
            return _promise->_tag == detail::tag::cancelled;
        }

        ~lazy_task()
        {
            reset();
        }

    private:

        promise_type* _promise;
    };

    template<class T>
    inline void swap(lazy_task<T>& a, lazy_task<T>& b) noexcept
    {
        a.swap(b);
    }
}

#endif