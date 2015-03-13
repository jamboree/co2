/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#include <thread>
#include <vector>
#include <map>
#include <chrono>
#include <iostream>
#include <boost/asio/io_service.hpp>
#include <co2/task.hpp>

auto post(boost::asio::io_service& io)
{
    struct awaiter
    {
        boost::asio::io_service& io;

        bool await_ready() const
        {
            return false;
        }

        void await_suspend(co2::coroutine<> const& cb)
        {
            io.post(cb);
        }

        void await_resume() {}
    };
    return awaiter{io};
}

std::thread::id tasks[1024];

auto task(boost::asio::io_service& io, std::thread::id& id) CO2_RET(co2::task<>, (io, id))
{
    using namespace std::chrono_literals;

    CO2_AWAIT(post(io));
    id = std::this_thread::get_id();
    std::this_thread::sleep_for(10ms);
} CO2_END

int main()
{
    boost::asio::io_service io;
    boost::asio::io_service::work work(io);
    std::map<std::thread::id, unsigned> stats;
    std::vector<std::thread> threads(std::thread::hardware_concurrency());
    for (auto& thread : threads)
    {
        thread = std::thread([&io] { io.run(); });
        stats[thread.get_id()];
    }
    std::cout << "threads: " << threads.size() << ", tasks: 1024\n";
    std::cout << "----------------------------------------------\n";

    for (auto& id : tasks)
        task(io, id);

    io.stop();
    for (auto& thread : threads)
        thread.join();

    for (auto id : tasks)
        ++stats[id];

    unsigned idx = 0;
    for (auto const& pair : stats)
        std::cout << "thread[" << idx++ << "]: " << pair.second << " tasks\n";

    return EXIT_SUCCESS;
}