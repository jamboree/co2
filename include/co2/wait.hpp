/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_WAIT_HPP_INCLUDED
#define CO2_WAIT_HPP_INCLUDED

#include <mutex>
#include <thread>
#include <cstddef>
#include <type_traits>
#include <condition_variable>
#include <co2/coroutine.hpp>
#include <co2/utility/fixed_allocator.hpp>

namespace co2 { namespace wait_detail
{
    // The promise_type will be allocated on stack, to ensure that it won't
    // be destroyed on the caller stack before the awaiter releases it, we
    // have to lock & notify_one in its destructor instead of in the coroutine
    // body, so the synchronization stuff are put inside the promise_type.
    struct result
    {
        struct promise_type
        {
            suspend_never initial_suspend()
            {
                return{};
            }

            void finalize() noexcept {}

            bool cancellation_requested() const
            {
                return false;
            }

            result get_return_object()
            {
                return {this};
            }

            void set_result() {}

            ~promise_type()
            {
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    not_ready = false;
                }
                cond.notify_one();
            }

            std::mutex mtx;
            std::condition_variable cond;
            bool not_ready = true;
        };
        promise_type* promise;
    };

    template<class Alloc, class Awaitable>
    auto run(Alloc alloc, Awaitable& a)
    CO2_RET(result, (a), CO2_RESERVE(sizeof(void*));)
    {
        CO2_AWAIT(awaken(a));
    } CO2_END_ALLOC(alloc)
}}

namespace co2
{
    template<class Awaitable>
    void wait(Awaitable&& a)
    {
        if (await_ready(a))
            return;
        constexpr auto space = sizeof(wait_detail::result::promise_type) + 6 * sizeof(void*);
        std::aligned_storage_t<space, alignof(std::max_align_t)> buf;
        auto promise = wait_detail::run(make_fixed_allocator(buf), a).promise;
        std::unique_lock<std::mutex> lock(promise->mtx);
        auto& cond = promise->cond;
        auto const& not_ready = promise->not_ready;
        while (not_ready) cond.wait(lock);
    }

    template<class Awaitable>
    inline decltype(auto) get(Awaitable&& a)
    {
        wait(a);
        return await_resume(a);
    }
}

#endif