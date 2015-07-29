/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_GENERATOR_HPP_INCLUDED
#define CO2_GENERATOR_HPP_INCLUDED

#include <co2/coroutine.hpp>
#include <co2/detail/storage.hpp>
#include <boost/iterator/iterator_facade.hpp>

namespace co2
{
    template<class T>
    struct generator
    {
        struct promise_type
        {
            using val_t = detail::wrap_reference_t<T>;

            generator get_return_object(coroutine<promise_type>& coro)
            {
                coro.resume();
                return generator(coro);
            }

            bool initial_suspend() noexcept
            {
                return true;
            }

            bool final_suspend() noexcept
            {
                reset_value();
                return false;
            }

            bool cancellation_requested() const noexcept
            {
                return false;
            }

            void set_result() noexcept {}

            suspend_always yield_value(T&& t)
            {
                return yield_value<T>(std::forward<T>(t));
            }

            template<class U>
            suspend_always yield_value(U&& u)
            {
                reset_value();
                _valid = true;
                new(&_data) val_t(std::forward<U>(u));
                return {};
            }

            T&& get()
            {
                return std::forward<T>(*reinterpret_cast<val_t*>(&_data));
            }

        private:

            void reset_value()
            {
                if (_valid)
                    reinterpret_cast<val_t*>(&_data)->~val_t();
            }
            
            detail::storage_for<val_t> _data;
            bool _valid = false;
        };

        struct iterator
          : boost::iterator_facade<iterator, T, std::input_iterator_tag, T&&>
        {
            iterator() : _coro() {}

            explicit iterator(coroutine<promise_type>& coro) : _coro(&coro)
            {
                increment();
            }

        private:

            friend class boost::iterator_core_access;

            void increment()
            {
                _coro->resume();
                if (!*_coro)
                    _coro = nullptr;
            }

            bool equal(iterator const& other) const
            {
                return _coro == other._coro;
            }

            T&& dereference() const
            {
                return _coro->promise().get();
            }

            coroutine<promise_type>* _coro;
        };

        generator() = default;

        generator(generator&& other) = default;

        generator& operator=(generator&& other) = default;

        void swap(generator& other) noexcept
        {
            _coro.swap(other._coro);
        }

        iterator begin()
        {
            if (!_coro)
                return {};
            return iterator(_coro);
        }

        iterator end()
        {
            return {};
        }

    private:

        explicit generator(coroutine<promise_type>& coro) : _coro(std::move(coro)) {}

        coroutine<promise_type> _coro;
    };

    template<class T>
    inline void swap(generator<T>& a, generator<T>& b) noexcept
    {
        a.swap(b);
    }
}

#endif