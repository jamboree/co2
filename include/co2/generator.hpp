/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_GENERATOR_HPP_INCLUDED
#define CO2_GENERATOR_HPP_INCLUDED

#include <co2/coroutine.hpp>
#include <boost/iterator/iterator_facade.hpp>

namespace co2 { namespace generator_detail
{
    enum class tag
    {
        null, value, exception
    };

    template<class T>
    union storage
    {
        std::exception_ptr exception;
        T value;

        storage() {}
        ~storage() {}
    };
}}

namespace co2
{
    template<class T>
    struct generator
    {
        struct promise_type
        {
            generator get_return_object()
            {
                return generator(*this);
            }

            suspend_always initial_suspend()
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

            void set_result() {}

            void set_exception(std::exception_ptr const& e)
            {
                _tag = generator_detail::tag::exception;
                new(&_data) std::exception_ptr(e);
            }

            suspend_always yield_value(T val)
            {
                _tag = generator_detail::tag::value;
                new(&_data) T(std::forward<T>(val));
                return {};
            }

            T&& get()
            {
                return std::forward<T>(_data.value);
            }

            void rethrow_exception()
            {
                if (_tag == generator_detail::tag::exception)
                    std::rethrow_exception(_data.exception);
            }

            ~promise_type()
            {
                switch (_tag)
                {
                case generator_detail::tag::value:
                    _data.value.~T();
                    break;
                case generator_detail::tag::exception:
                    _data.exception.~exception_ptr();
                default:
                    break;
                }
            }

        private:

            generator_detail::storage<T> _data;
            generator_detail::tag _tag = generator_detail::tag::null;
        };

        struct iterator
          : boost::iterator_facade<iterator, T, std::input_iterator_tag, T&&>
        {
            iterator() = default;

            explicit iterator(coroutine<promise_type> const& coro)
              : _coro(coro)
            {
                increment();
            }

        private:

            friend class boost::iterator_core_access;

            void increment()
            {
                _coro();
                _coro.promise().rethrow_exception();
                if (_coro.done())
                    _coro.reset();
            }

            bool equal(iterator const& other) const
            {
                return _coro == other._coro;
            }

            T&& dereference() const
            {
                return _coro.promise().get();
            }

            coroutine<promise_type> _coro;
        };

        generator() : _coro(get_empty_frame()) {}

        explicit generator(promise_type& p) : _coro(&p) {}

        generator(generator&& other) = default;

        generator& operator=(generator&& other) = default;

        iterator begin()
        {
            if (_coro.done())
                return {};
            return iterator(_coro);
        }

        iterator end()
        {
            return {};
        }

    private:

        struct empty_frame final : detail::resumable<promise_type>
        {
            empty_frame()
            {
                this->_next = detail::sentinel::value;
            }

            void run(coroutine<> const& coro) noexcept override {}

            void release(coroutine<> const& coro) noexcept override {}
        };

        static empty_frame* get_empty_frame()
        {
            static empty_frame ret;
            return &ret;
        }

        coroutine<promise_type> _coro;
    };
}

#endif