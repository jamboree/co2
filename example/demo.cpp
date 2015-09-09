#include <iostream>
#include <co2/coroutine.hpp>
#include <co2/generator.hpp>
#include <co2/recursive_generator.hpp>
#include <co2/utility/stack_allocator.hpp>

auto coro(int i, int e) CO2_BEG(co2::coroutine<>, (i, e))
{
    for (; i != e; ++i)
    {
        CO2_AWAIT(co2::suspend_always{});
        std::cout << i;
    }
} CO2_END

auto range(int i, int e) CO2_BEG(co2::generator<int>, (i, e))
{
    for (; i != e; ++i)
        CO2_YIELD(i);
} CO2_END

template<class Alloc>
auto recursive_range(Alloc alloc, int a, int b)
CO2_BEG(co2::recursive_generator<int>, (alloc, a, b) new(alloc),
    int n = b - a;
)
{
    if (n <= 0)
        CO2_RETURN();

    if (n == 1)
    {
        CO2_YIELD(a);
        CO2_RETURN();
    }

    n = a + n / 2;
    CO2_YIELD(recursive_range(alloc, a, n));
    CO2_YIELD(recursive_range(alloc, n, b));
} CO2_END

int main()
{
    std::cout << "[coro]\n";
    auto c = coro(1, 10);
    while (c)
    {
        c();
        std::cout << ", ";
    }
    std::cout << "\n------------\n";

    std::cout << "[range]\n";
    for (auto i : range(1, 10))
    {
        std::cout << i << ", ";
    }
    std::cout << "\n------------\n";

    std::cout << "[recursive_range]\n";
    co2::stack_buffer<64 * 1024> buf;
    co2::stack_allocator<> alloc(buf);
    for (auto i : recursive_range(alloc, 1, 10))
    {
        std::cout << i << ", ";
    }
    std::cout << "\n------------\n";
    return EXIT_SUCCESS;
}