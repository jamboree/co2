/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_FIXED_ALLOCATOR_HPP_INCLUDED
#define CO2_FIXED_ALLOCATOR_HPP_INCLUDED

namespace co2
{
    template<class T, class Space>
    struct fixed_allocator : fixed_allocator<void, Space>
    {
        using value_type = T;

        using base_type = fixed_allocator<void, Space>;

        using base_type::base_type;

        fixed_allocator(base_type const& other) : base_type(other) {}

        T* allocate(std::size_t n)
        {
            static_assert(sizeof(T) <= sizeof(Space), "insufficient space");
            return reinterpret_cast<T*>(&this->space);
        }

        void deallocate(T* p, std::size_t n) noexcept {}
    };

    template<class Space>
    struct fixed_allocator<void, Space>
    {
        using value_type = void;

        fixed_allocator(Space& space) noexcept : space(space) {}
        fixed_allocator(fixed_allocator const&) = default;
        fixed_allocator& operator=(fixed_allocator const&) = delete;

        bool operator==(fixed_allocator const& other) noexcept
        {
            return &space == &other.space;
        }

        bool operator!=(fixed_allocator const& other) noexcept
        {
            return &space != &other.space;
        }

        Space& space;
    };

    template<class Space>
    inline fixed_allocator<void, Space> make_fixed_allocator(Space& space)
    {
        return fixed_allocator<void, Space>(space);
    }
}

#endif