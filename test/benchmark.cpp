#include <benchmark/benchmark.h>
#include <co2/coroutine.hpp>
#include <boost/coroutine2/coroutine.hpp>
#include <experimental/coroutine>

void run_coroutines2(boost::coroutines2::coroutine<void>::push_type& c)
{
    for (;;)
        c();
}

void bench_coroutines2(benchmark::State& state)
{
    boost::coroutines2::coroutine<void>::pull_type c(run_coroutines2);
    while (state.KeepRunning())
        c();
}

auto run_co2() CO2_BEG(co2::coroutine<>, ())
{
    for (;;)
        CO2_SUSPEND([](co2::coroutine<>&) {});
} CO2_END

void bench_co2(benchmark::State& state)
{
    auto c = run_co2();
    while (state.KeepRunning())
        c();
}

BENCHMARK(bench_coroutines2);
BENCHMARK(bench_co2);

#ifdef _RESUMABLE_FUNCTIONS_SUPPORTED
namespace std { namespace experimental
{
    template<class... Ts>
    struct coroutine_traits<coroutine_handle<>, Ts...>
    {
        struct promise_type
        {
            bool initial_suspend() noexcept
            {
                return false;
            }

            bool final_suspend() noexcept
            {
                return false;
            }

            coroutine_handle<> get_return_object()
            {
                return coroutine_handle<promise_type>::from_promise(*this);
            }

            void return_void() noexcept {}
        };
    };
}}

auto run_msvc_await() -> std::experimental::coroutine_handle<>
{
    for (;;)
        co_await std::experimental::suspend_always{};
}

void bench_msvc(benchmark::State& state)
{
    auto c = run_msvc_await();
    while (state.KeepRunning())
        c();
    c.destroy();
}

BENCHMARK(bench_msvc);
#endif

BENCHMARK_MAIN();