/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_ORNION_HPP_INCLUDED
#define CO2_ORNION_HPP_INCLUDED

#include <cstddef>
#include <type_traits>
#include <co2/detail/void.hpp>

namespace co2 { namespace ornion_detail
{
    template<std::size_t N, class... T>
    struct list
    {
        static void at();
    };

    template<std::size_t N, class T, class... Ts>
    struct list<N, T, Ts...> : list<N + 1, Ts...>
    {
        using list<N + 1, Ts...>::at;

        static T at(std::integral_constant<std::size_t, N>);
    };

    template<class... T>
    struct switcher;

    template<class T1>
    struct switcher<T1>
    {
        template<class F>
        static void apply(std::size_t n, F&& f)
        {
            return f(T1{});
        }
    };

    template<class T1, class T2>
    struct switcher<T1, T2>
    {
        template<class F>
        static void apply(std::size_t n, F&& f)
        {
            switch (n)
            {
            case 0: return f(T1{});
            case 1: return f(T2{});
            }
        }
    };

    template<class T1, class T2, class T3>
    struct switcher<T1, T2, T3>
    {
        template<class F>
        static void apply(std::size_t n, F&& f)
        {
            switch (n)
            {
            case 0: return f(T1{});
            case 1: return f(T2{});
            case 2: return f(T3{});
            }
        }
    };

    template<class T>
    struct tag
    {
        using type = T;
    };

    template<>
    struct tag<void>
    {
        using type = detail::void_;
    };

    template<class... T>
    using storage = std::aligned_union_t<0, std::exception_ptr, typename tag<T>::type...>;

    template<std::size_t N, class... T>
    using type_at = typename tag<decltype(list<0, void, T...>::at(std::integral_constant<std::size_t, N + 1>{}))>::type;

    template<class IndexSeq>
    struct index_switcher;

    template<std::size_t... N>
    struct index_switcher<std::index_sequence<N...>>
    {
        using type = switcher<std::integral_constant<std::size_t, N>...>;
    };

    struct reset_fn
    {
        void* p;

        template<class Tag>
        void operator()(Tag) const
        {
            using type = typename Tag::type;
            static_cast<type*>(p)->~type();
        }
    };

    struct move_fn
    {
        void* src;
        void* dst;

        template<class Tag>
        void operator()(Tag) const
        {
            using type = typename Tag::type;
            new(dst) type(std::move(*static_cast<type*>(src)));
        }
    };

    struct copy_fn
    {
        void const* src;
        void* dst;

        template<class Tag>
        void operator()(Tag) const
        {
            using type = typename Tag::type;
            new(dst) type(*static_cast<type const*>(src));
        }
    };
}}

namespace co2
{
    template<class... T>
    struct ornion
    {
        using dispatch = ornion_detail::switcher<
            ornion_detail::tag<void>, ornion_detail::tag<T>...>;

        template<class... U>
        friend struct ornion;

        ornion() : _which(0) {}

        ornion(ornion&& other) : _which(other._which)
        {
            if (_which & 1)
                new(&_data) std::exception_ptr(reinterpret_cast<std::exception_ptr&&>(other._data));
            else
                dispatch::apply(_which >> 1, ornion_detail::move_fn{&other._data, &_data});
        }

        ornion(ornion const& other) : _which(other._which)
        {
            if (_which & 1)
                new(&_data) std::exception_ptr(reinterpret_cast<std::exception_ptr const&>(other._data));
            else
                dispatch::apply(_which >> 1, ornion_detail::copy_fn{&other._data, &_data});
        }

        template<class... U>
        ornion(ornion<U...>&& other) : _which(other._which)
        {
            using index_dispatch = typename
                ornion_detail::index_switcher<std::make_index_sequence<1 + sizeof...(T)>>::type;
            if (_which & 1)
                new(&_data) std::exception_ptr(reinterpret_cast<std::exception_ptr&&>(other._data));
            else
                index_dispatch::apply(_which >> 1, [src = &other._data, dst = &_data](auto idx)
                {
                    using namespace ornion_detail;
                    using from = typename tag<decltype(list<0, void, U...>::at(idx))>::type;
                    using to = typename tag<decltype(list<0, void, T...>::at(idx))>::type;
                    new(dst) to(std::move(*reinterpret_cast<from*>(src)));
                });
        }

        template<class... U>
        ornion(ornion<U...> const& other) : _which(other._which)
        {
            using index_dispatch = typename
                ornion_detail::index_switcher<std::make_index_sequence<1 + sizeof...(T)>>::type;
            if (_which & 1)
                new(&_data) std::exception_ptr(reinterpret_cast<std::exception_ptr const&>(other._data));
            else
                index_dispatch::apply(_which >> 1, [src = &other._data, dst = &_data](auto idx)
                {
                    using namespace ornion_detail;
                    using from = typename tag<decltype(list<0, void, U...>::at(idx))>::type;
                    using to = typename tag<decltype(list<0, void, T...>::at(idx))>::type;
                    new(dst) to(*reinterpret_cast<from const*>(src));
                });
        }

        ornion& operator=(ornion&& other)
        {
            this->~ornion();
            return *new(this) ornion(std::move(other));
        }

        ornion& operator=(ornion const& other)
        {
            this->~ornion();
            return *new(this) ornion(other);
        }

        ~ornion()
        {
            reset();
        }

        int which() const
        {
            return int(_which >> 1) - 1;
        }

        template<std::size_t N>
        decltype(auto) get()
        {
            using type = ornion_detail::type_at<N, T...>;
            if (_which & 1)
                std::rethrow_exception(reinterpret_cast<std::exception_ptr&>(_data));
            return reinterpret_cast<type&>(_data), detail::void_{};
        }

        template<std::size_t N, class U>
        void set_value(U&& u)
        {
            using type = ornion_detail::type_at<N, T...>;
            reset();
            new(&_data) type(std::forward<U>(u));
            _which = (N + 1) << 1;
        }

        template<std::size_t N>
        void set_exception(std::exception_ptr const& e)
        {
            reset();
            new(&_data) std::exception_ptr(e);
            _which = ((N + 1) << 1) | 1;
        }

        void reset()
        {
            if (_which & 1)
                reinterpret_cast<std::exception_ptr&>(_data).~exception_ptr();
            else
                dispatch::apply(_which >> 1, ornion_detail::reset_fn{&_data});
            _which = 0;
        }

    private:

        ornion_detail::storage<T...> _data;
        unsigned _which;
    };

    template<std::size_t N, class... T, class U>
    inline void set_value(ornion<T...>& any, U&& u)
    {
        any.template set_value<N>(std::forward<U>(u));
    }

    template<std::size_t N, class... T>
    inline void set_exception(ornion<T...>& any, std::exception_ptr const& e)
    {
        any.template set_exception<N>(e);
    }

    template<std::size_t N, class... T>
    inline decltype(auto) get(ornion<T...>& any)
    {
        return any.template get<N>();
    }
}

#endif