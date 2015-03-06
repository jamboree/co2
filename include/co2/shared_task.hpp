/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SHARED_TASK_HPP_INCLUDED
#define CO2_SHARED_TASK_HPP_INCLUDED

#include <atomic>
#include <vector>
#include <type_traits>
#include <co2/coroutine.hpp>
#include <co2/detail/task.hpp>

namespace co2 { namespace task_detail
{
    struct unlock_at_exit
    {
        std::atomic<unsigned>& lock;

        ~unlock_at_exit()
        {
            lock.store(0, std::memory_order_release);
        }
    };

    struct shared_promise_base : promise_base
    {
        std::atomic<unsigned> _lock {0};
        std::atomic<tag> _tag {tag::null};
        std::vector<coroutine<>> _followers;

        void notify()
        {
            while (_lock.exchange(2, std::memory_order_acquire));
            unlock_at_exit _{_lock};
            for (auto& f : _followers)
                f();
            _followers.clear();
            _followers.shrink_to_fit();
        }

        bool follow(coroutine<> const& cb)
        {
            unsigned flag;
            do
            {
                flag = _lock.fetch_or(1, std::memory_order_acquire);
                if (flag & 2)
                    return false;
            } while (flag);
            unlock_at_exit _{_lock};
            _followers.push_back(cb);
            return true;
        }
    };

    template<class T>
    using shared_t = std::add_lvalue_reference_t<std::add_const_t<T>>;
}}

namespace co2
{
    template<class T = void>
    struct shared_task
      : task_detail::impl<shared_task<T>, T, task_detail::shared_t<T>, task_detail::shared_promise_base>
    {
        using base_type = task_detail::impl<shared_task<T>, T, task_detail::shared_t<T>, task_detail::shared_promise_base>;

        using base_type::base_type;
    };

    template<class T>
    inline void swap(shared_task<T>& a, shared_task<T>& b) noexcept
    {
        a.swap(b);
    }
}

#endif
