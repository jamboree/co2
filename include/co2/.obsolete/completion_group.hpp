/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_SYNC_COMPLETION_GROUP_HPP_INCLUDED
#define CO2_SYNC_COMPLETION_GROUP_HPP_INCLUDED

#include <atomic>
#include <co2/coroutine.hpp>
#include <co2/detail/fixed_storage.hpp>

namespace co2 { namespace detail
{
    struct completion_group
    {
        struct promise_type
        {
            std::atomic<coroutine_handle> _then{nullptr};
            std::atomic<unsigned> _task_count{1u};

            completion_group get_return_object(coroutine<promise_type>& coro)
            {
                coro.detach();
                return {this};
            }

            bool initial_suspend() noexcept
            {
                return false;
            }

            bool final_suspend() noexcept
            {
                if (auto then = _then.exchange(nullptr, std::memory_order_relaxed))
                    coroutine_final_run(then);
                return true;
            }

            bool cancellation_requested() const noexcept
            {
                return false;
            }

            void set_result() noexcept {}

            bool resume() noexcept
            {
                return _task_count.fetch_sub(1u, std::memory_order_relaxed) == 1u;
            }

            bool follow(coroutine<>& cb)
            {
                _then.store(cb.handle(), std::memory_order_relaxed);
                if (_task_count.load(std::memory_order_relaxed)
                    || !_then.exchange(nullptr, std::memory_order_relaxed))
                {
                    cb.detach();
                    return true;
                }
                return false;
            }
        };

        using frame_storage = fixed_storage<frame_size<
            promise_type, char, fixed_allocator_base, 0>>;

        promise_type* promise;
    };

    template<class Alloc>
    auto create_completion_group(Alloc alloc) CO2_BEG(completion_group, () new(alloc), CO2_TEMP_SIZE(0);)
    {} CO2_END
}}

namespace co2
{
    class completion_group
    {
        using promise_type = detail::completion_group::promise_type;

        struct awaitable
        {
            promise_type* _promise;

            bool await_ready() noexcept
            {
                return !_promise;
            }

            bool await_suspend(coroutine<>& cb) noexcept
            {
                return _promise->follow(cb);
            }

            void await_resume() noexcept {}
        };

        detail::completion_group::frame_storage _storage;
        promise_type* _promise;

    public:
        completion_group() noexcept
          : _promise(detail::create_completion_group(_storage.alloc()).promise)
        {}

        completion_group(completion_group&&) = delete;

        completion_group& operator=(completion_group&&) = delete;

        ~completion_group()
        {
            coroutine<promise_type>::destroy(_promise);
        }

        template<class Task>
        void add(Task&& task)
        {
            if (co2::await_ready(task))
                return;
            _promise->_task_count.fetch_add(1u, std::memory_order_relaxed);
            coroutine<promise_type> coro(_promise);
            co2::await_suspend(task, coro);
        }

        awaitable join()
        {
            return {_promise->resume() ? nullptr : _promise};
        }

        void reset()
        {
            coroutine<promise_type>::destroy(_promise);
            _promise = detail::create_completion_group(_storage.alloc()).promise;
        }
    };
}

#endif