/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_RECURSIVE_GENERATOR_HPP_INCLUDED
#define CO2_RECURSIVE_GENERATOR_HPP_INCLUDED

#include <co2/coroutine.hpp>
#include <co2/detail/storage.hpp>
#include <boost/iterator/iterator_facade.hpp>

namespace co2
{
    template<class T>
    struct recursive_generator
    {
        struct promise_type
        {
            using val_t = detail::wrap_reference_t<T>;

            recursive_generator get_return_object()
            {
                return recursive_generator(_head);
            }

            suspend_always initial_suspend()
            {
                return {};
            }

            void finalize() noexcept
            {
                if (_head != &_parent)
                {
                    _head->swap(_parent);
                    (*_head)();
                }
            }

            bool cancellation_requested() const
            {
                return false;
            }

            void set_result()
            {
                reset_value();
            }

            void set_exception(std::exception_ptr const& e)
            {
                reset_value();
                _tag = detail::tag::exception;
                new(&_data) std::exception_ptr(e);
            }

            suspend_always yield_value(T&& t)
            {
                return yield_value<T>(std::forward<T>(t));
            }

            template<class U>
            suspend_always yield_value(U&& u)
            {
                reset_value();
                _tag = detail::tag::value;
                new(&_data) val_t(std::forward<U>(u));
                return {};
            }

            auto yield_value(recursive_generator<T>&& child)
            {
                return yield_from(std::move(child));
            }

            auto yield_value(recursive_generator<T>& child)
            {
                return yield_from(child);
            }

            T&& get()
            {
                return std::forward<T>(_data.value);
            }

            void rethrow_exception()
            {
                if (_tag == detail::tag::exception)
                {
                    auto ex(std::move(_data.exception));
                    _data.exception.~exception_ptr();
                    std::rethrow_exception(std::move(ex));
                }
            }

        private:

            void reset_value()
            {
                if (_tag == detail::tag::value)
                    _data.value.~val_t();
            }

            template<class Generator>
            static auto yield_from(Generator&& child)
            {
                struct awaiter
                {
                    bool await_ready() noexcept
                    {
                        return !_child._coro || _child._coro->done();
                    }
                
                    void await_suspend(coroutine<promise_type> const& coro) noexcept
                    {
                        auto head = coro.promise()._head;
                        _child._coro->promise()._head = head;
                        head->swap(*_child._coro);
                        (*head)();
                    }
                
                    void await_resume()
                    {
                        if (_child._coro)
                            _child._coro->promise().rethrow_exception();
                    }
                
                    Generator _child;
                };
                return awaiter{std::forward<Generator>(child)};
            }
            
            coroutine<promise_type>* _head = &_parent;
            coroutine<promise_type> _parent {this};
            detail::storage<val_t> _data;
            detail::tag _tag = detail::tag::null;
        };

        struct iterator
          : boost::iterator_facade<iterator, T, std::input_iterator_tag, T&&>
        {
            iterator() : _coro() {}

            explicit iterator(coroutine<promise_type> const* coro)
              : _coro(coro)
            {
                increment();
            }

        private:

            friend class boost::iterator_core_access;

            void increment()
            {
                (*_coro)();
                _coro->promise().rethrow_exception();
                if (_coro->done())
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

            coroutine<promise_type> const* _coro;
        };

        recursive_generator() noexcept : _coro() {}

        recursive_generator(recursive_generator&& other) noexcept
          : _coro(other._coro)
        {
            other._coro = nullptr;
        }

        recursive_generator& operator=(recursive_generator&& other) noexcept
        {
            this->~recursive_generator();
            return *new(this) recursive_generator(std::move(other));
        }

        ~recursive_generator()
        {
            if (_coro)
                _coro->reset();
        }

        void swap(recursive_generator& other) noexcept
        {
            _coro.swap(other._coro);
        }

        iterator begin()
        {
            if (!_coro || _coro->done())
                return {};
            return iterator(_coro);
        }

        iterator end()
        {
            return {};
        }

    private:

        explicit recursive_generator(coroutine<promise_type>* coro) noexcept
          : _coro(coro)
        {}

        coroutine<promise_type>* _coro;
    };

    template<class T>
    inline void swap(recursive_generator<T>& a, recursive_generator<T>& b) noexcept
    {
        a.swap(b);
    }
}

#endif