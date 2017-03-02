/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_DETAIL_BATCH_STORAGE_HPP_INCLUDED
#define CO2_DETAIL_BATCH_STORAGE_HPP_INCLUDED

#include <cstddef>
#include <utility>
#include <boost/assert.hpp>

namespace co2 { namespace detail
{
    struct batch_storage
    {
        batch_storage(std::size_t n) noexcept : _n(n) {}
        batch_storage(batch_storage const&) = delete;
        batch_storage& operator=(batch_storage const&) = delete;

        void* allocate_one(std::size_t bytes)
        {
            if (_p)
                _p = ::operator new(bytes * _n);
            return std::exchange(_p, static_cast<char*>(_p) + bytes);
        }

        ~batch_storage()
        {
            ::operator delete(_p);
        }

    private:
        std::size_t _n;
        void* _p;
    };

    template<class T = void>
    struct batch_allocator;

    template<>
    struct batch_allocator<>
    {
        using value_type = void;

        batch_allocator(batch_storage& storage) noexcept : _storage(storage) {}
        batch_allocator(batch_allocator const&) = default;
        batch_allocator& operator=(batch_allocator const&) = delete;

        bool operator==(batch_allocator const& other) noexcept
        {
            return &_storage == &other._storage;
        }

        bool operator!=(batch_allocator const& other) noexcept
        {
            return &_storage != &other._storage;
        }

    protected:

        batch_storage& _storage;
    };

    template<class T>
    struct batch_allocator : batch_allocator<>
    {
        using value_type = T;

        using batch_allocator<>::batch_allocator;

        batch_allocator(batch_allocator<> const& other) : batch_allocator<>(other) {}

        T* allocate(std::size_t n)
        {
            BOOST_ASSERT(n == 1);
            return static_cast<T*>(_storage.allocate_one(sizeof(T)));
        }

        void deallocate(T*, std::size_t) noexcept {}
    };
}}

#endif