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

            recursive_generator get_return_object(coroutine<promise_type>& coro)
            {
                _parent = std::move(coro);
                _parent();
                return recursive_generator(this);
            }

            bool initial_suspend() noexcept
            {
                return true;
            }

            bool final_suspend() noexcept
            {
                if (_head != &_parent)
                {
                    _head->swap(_parent);
                    (*_head)();
                }
                return true;
            }

            bool cancellation_requested() const noexcept
            {
                return false;
            }

            void set_result()
            {
                reset_value();
            }

            void cancel()
            {
                reset_value();
            }

            void set_exception(std::exception_ptr e)
            {
                reset_value();
                _tag = detail::tag::exception;
                new(&_data) std::exception_ptr(std::move(e));
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

            coroutine<promise_type>& current()
            {
                return _parent;
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
                        return !_child._promise;
                    }
                
                    void await_suspend(coroutine<promise_type> const& coro) noexcept
                    {
                        auto head = coro.promise()._head;
                        head->swap(_child._promise->_parent);
                        head->promise()._head = head;
                        (*head)();
                    }
                
                    void await_resume()
                    {
                        if (_child._promise)
                            _child._promise->rethrow_exception();
                    }
                
                    Generator _child;
                };
                return awaiter{std::forward<Generator>(child)};
            }
            
            coroutine<promise_type>* _head = &_parent;
            coroutine<promise_type> _parent;
            detail::storage<val_t> _data;
            detail::tag _tag = detail::tag::null;
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
                auto& promise = _coro->promise();
                (*_coro)();
                promise.rethrow_exception();
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

        recursive_generator() noexcept : _promise() {}

        recursive_generator(recursive_generator&& other) noexcept
          : _promise(other._promise)
        {
            other._promise = nullptr;
        }

        recursive_generator& operator=(recursive_generator other) noexcept
        {
            this->~recursive_generator();
            return *new(this) recursive_generator(std::move(other));
        }

        ~recursive_generator()
        {
            if (_promise)
            {
                _promise->current().reset();
                coroutine<promise_type>::destroy(_promise);
            }
        }

        void swap(recursive_generator& other) noexcept
        {
            std::swap(_promise, other._promise);
        }

        iterator begin()
        {
            if (_promise)
            {
                if (auto& coro = _promise->current())
                    return iterator(coro);
            }
            return {};
        }

        iterator end()
        {
            return {};
        }

    private:

        explicit recursive_generator(promise_type* promise) noexcept
          : _promise(promise)
        {}

        promise_type* _promise;
    };

    template<class T>
    inline void swap(recursive_generator<T>& a, recursive_generator<T>& b) noexcept
    {
        a.swap(b);
    }
}

#endif