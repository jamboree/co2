/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015-2016 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_STACK_ALLOCATOR_HPP_INCLUDED
#define CO2_STACK_ALLOCATOR_HPP_INCLUDED

#include <cstddef>

namespace co2
{
    struct stack_manager
    {
        stack_manager(void* data, std::size_t n) noexcept
          : _beg(static_cast<char*>(data)), _ptr(_beg), _end(_beg + n)
        {}

        stack_manager(stack_manager const&) = delete;
        stack_manager& operator=(stack_manager const&) = delete;

        void* allocate(std::size_t n)
        {
            if (std::size_t(_end - _ptr) < n)
                return ::operator new(n);
            auto p = _ptr;
            _ptr += n;
            return p;
        }

        void deallocate(void* ptr, std::size_t n) noexcept
        {
            auto p = static_cast<char*>(ptr);
            if (contains(p))
            {
                if (p + n == _ptr)
                    _ptr = p;
            }
            else
                ::operator delete(p);
        }

        std::size_t used() const noexcept
        {
            return _ptr - _beg;
        }

        void clear() noexcept
        {
            _ptr = _beg;
        }

    private:

        bool contains(char* p) noexcept
        {
            return _beg <= p && p < _end;
        }

        char* const _beg;
        char* _ptr;
        char* const _end;
    };

    template<std::size_t Bytes>
    struct stack_buffer : stack_manager
    {
        stack_buffer() : stack_manager(_data, Bytes) {}

    private:

        alignas(std::max_align_t) char _data[Bytes];
    };

    template<class T = void>
    struct stack_allocator;

    template<>
    struct stack_allocator<>
    {
        using value_type = void;

        stack_allocator(stack_manager& manager) noexcept : _manager(manager) {}
        stack_allocator(stack_allocator const&) = default;
        stack_allocator& operator=(stack_allocator const&) = delete;

        bool operator==(stack_allocator const& other) noexcept
        {
            return &_manager == &other._manager;
        }

        bool operator!=(stack_allocator const& other) noexcept
        {
            return &_manager != &other._manager;
        }

    protected:

        stack_manager& _manager;
    };

    template<class T>
    struct stack_allocator : stack_allocator<>
    {
        using value_type = T;

        using stack_allocator<>::stack_allocator;

        stack_allocator(stack_allocator<> const& other) : stack_allocator<>(other) {}

        T* allocate(std::size_t n)
        {
            return static_cast<T*>(_manager.allocate(n * sizeof(T)));
        }

        void deallocate(T* p, std::size_t n) noexcept
        {
            _manager.deallocate(p, n * sizeof(T));
        }
    };
}

#endif