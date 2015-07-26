/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_DETAIL_ITERATOR_HPP_INCLUDED
#define CO2_DETAIL_ITERATOR_HPP_INCLUDED

#include <boost/iterator/iterator_facade.hpp>

namespace co2 { namespace detail
{
    template<class T, class Coro>
    struct iterator
      : boost::iterator_facade<iterator<T, Coro>, T, std::input_iterator_tag, T&&>
    {
        iterator() : _coro() {}

        explicit iterator(Coro& coro) : _coro(&coro)
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

        Coro* _coro;
    };
}}

#endif