/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SYNC_WORK_GROUP_HPP_INCLUDED
#define CO2_SYNC_WORK_GROUP_HPP_INCLUDED

#include <atomic>
#include <memory>
#include <boost/assert.hpp>
#include <co2/coroutine.hpp>

namespace co2
{
    class work_group
    {
        std::atomic<void*> _then{nullptr};
        std::atomic<unsigned> _work_count{0};

        void push_work() noexcept
        {
            _work_count.fetch_add(1u, std::memory_order_release);
        }

        void pop_work() noexcept
        {
            if (_work_count.fetch_sub(1u, std::memory_order_relaxed) == 1u)
            {
                if (auto then = _then.exchange(this, std::memory_order_acquire))
                    coroutine<>{static_cast<coroutine_handle>(then)}();
            }
        }

        struct work_deleter
        {
            void operator()(work_group* group) const noexcept
            {
                group->pop_work();
            }
        };

    public:
        class work
        {
            std::unique_ptr<work_group, work_deleter> _group;

        public:
            explicit work(work_group& group) noexcept : _group(&group)
            {
                group.push_work();
            }
        };

        ~work_group()
        {
            BOOST_ASSERT_MSG(await_ready(), "pending work in work_group");
        }

        work create()
        {
            return work(*this);
        }

        bool await_ready() noexcept
        {
            return !_work_count.load(std::memory_order_relaxed);
        }

        bool await_suspend(coroutine<>& cb) noexcept
        {
            if (_then.exchange(cb.handle(), std::memory_order_release))
                return false;
            cb.detach();
            return true;
        }

        void await_resume() noexcept
        {
            _then.store(nullptr, std::memory_order_relaxed);
        }
    };
}

#endif