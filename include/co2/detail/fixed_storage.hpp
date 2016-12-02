/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2016 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_DETAIL_FIXED_STORAGE_HPP_INCLUDED
#define CO2_DETAIL_FIXED_STORAGE_HPP_INCLUDED

#include <cstddef>

namespace co2 { namespace detail
{
    struct fixed_allocator_base
    {
        explicit fixed_allocator_base(void* data) noexcept : data(data) {}
        fixed_allocator_base(fixed_allocator_base const&) = default;
        fixed_allocator_base& operator=(fixed_allocator_base const&) = delete;

        bool operator==(fixed_allocator_base const& other) noexcept
        {
            return data == other.data;
        }

        bool operator!=(fixed_allocator_base const& other) noexcept
        {
            return data != other.data;
        }

        void* data;
    };

    template<std::size_t N>
    struct fixed_storage
    {
        alignas(std::max_align_t) char data[N];

        template<class T = void>
        struct allocator : fixed_allocator_base
        {
            using value_type = T;

            using fixed_allocator_base::fixed_allocator_base;

            explicit allocator(fixed_storage& m) noexcept : fixed_allocator_base(m.data) {}

            allocator(fixed_allocator_base const& other) noexcept : fixed_allocator_base(other) {}

            T* allocate(std::size_t) noexcept
            {
                static_assert(sizeof(T) <= N, "insufficient memory size");
                return static_cast<T*>(data);
            }

            void deallocate(T*, std::size_t) noexcept {}
        };

        allocator<> alloc()
        {
            return allocator<>(*this);
        }
    };
}}

#endif