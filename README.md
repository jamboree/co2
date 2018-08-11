CO2 - Coroutine II [![Try it online][badge.wandbox]](https://wandbox.org/permlink/hl2KlNuVWPFwggGE)
===

A header-only C++ stackless coroutine emulation library, providing interface close to [N4286](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4286.pdf).

## Requirements

- C++14
- [Boost](http://www.boost.org/)

## Overview

Many of the concepts are similar to N4286, if you're not familiar with the proposal, please read the paper first.

A coroutine written in this library looks like below:
```c++
auto function(Args... args) CO2_BEG(return_type, (args...), locals...)
{
    <coroutine-body>
} CO2_END
```

`function` is really just a plain-old function, you can forward declare it as usual:
```c++
auto function(Args... args) -> return_type;
return_type function(Args... args); // same as above
```

Of course, lambda expressions can be used as well:
```c++
[](Args... args) CO2_BEG(return_type, (args...), locals...)
{
    <coroutine-body>
} CO2_END
```

The coroutine body has to be surrounded with 2 macros: `CO2_BEG` and `CO2_END`.

The macro `CO2_BEG` requires you to provide some parameters:
* _return-type_ - the function's return-type, e.g. `co2::task<>`
* _captures_ - a list of comma separated args with an optional `new` clause, e.g. `(a, b) new(alloc)`
* _locals_ - a list of local-variable definitions, e.g. `int a;`

If there's no _captures_ and _locals_, it looks like:
```c++
CO2_BEG(return_type, ())
```

You can intialize the local variables as below:
```c++
auto f(int i) CO2_BEG(return_type, (i),
    int i2 = i * 2;
    std::string msg{"hello"};
)
{
    // coroutine-body
} CO2_END
```

Note that the `()` initializer cannot be used here, e.g. `int i2(i * 2);`, due to some emulation restrictions.
Besides, `auto` deduced variable cannot be used directly, i.e. `auto var{expr};`, you have to use `CO2_AUTO(var, expr);` instead.

Note that in this emulation, local variables intialization happens before `initial_suspend`, and if any exception is thrown during the intialization, `set_exception` won't be called, instead, the exception will propagate to the caller directly.

By default, the library allocates memory for coroutines using `std::allocator`, you can specify the allocator by appending the `new` clause after the args-list, for example:

```c++
template<class Alloc>
auto coro(Alloc alloc, int i) CO2_BEG(return_type, (i) new(alloc))
```

The `alloc` doesn't have to appear in the args-list if it's not used inside the coroutine-body. The `new` clause accepts an expression that evaluates to an _Allocator_, it's not restricted to identifiers as in the args-list.

Inside the coroutine body, there are some restrictions:
* local variables with automatic storage cannot cross suspend-resume points - you should specify them in local variables section of `CO2_BEG` as described above
* `return` should be replaced with `CO2_RETURN`/`CO2_RETURN_FROM`/`CO2_RETURN_LOCAL`
* try-catch block surrouding suspend-resume points should be replaced with `CO2_TRY` & `CO2_CATCH`
* identifiers starting with `_co2_` are reserved for this library

After defining the coroutine body, remember to close it with `CO2_END`.

### await & yield

In _CO2_, `await` is implemented as a statement instead of an expression due to the emulation limitation, and it has 4 variants: `CO2_AWAIT`, `CO2_AWAIT_SET`, `CO2_AWAIT_LET` and `CO2_AWAIT_RETURN`.

* `CO2_AWAIT(expr)`

Equivalent to `await expr`.

* `CO2_AWAIT_SET(var, expr)`

Equivalent to `var = await expr`.

* `CO2_AWAIT_LET(var-decl, expr, body)`

This allows you bind the awaited result to a temporary and do something to it.
```c++
CO2_AWAIT_LET(auto i, task,
{
    doSomething(i);
});
```

* `CO2_AWAIT_RETURN(expr)`

Equivalent to `return await expr`.

* `CO2_AWAIT_APPLY(f, expr)`

Equivalent to `f(await expr)`, where `f` can be a unary function or macro.

> *Note* -
> If your compiler supports _Statement Expression_ extension (e.g. GCC & Clang), you can use `CO2_AWAIT` as an expression.
However, don't use more than one `CO2_AWAIT` in a single statement, and don't use it as an argument of a function in company with other arguments.

* `CO2_YIELD(expr)`

Equivalent to `CO2_AWAIT(<this-promise>.yield_value(expr))`, as how `yield` is defined in N4286.

* `CO2_SUSPEND(fn)`

Suspend the coroutine with the callable object `fn`. This signature of `fn` is the same as `await_suspend`.


The fact that `await` in _CO2_ is not an expression has an implication on object lifetime, consider this case:

`await something{temporaries}` and `something` holds references to temporaries.

It's safe if `await` is an expression as in N4286, but in _CO2_, `CO2_AWAIT(something{temporaries})` is an emulated statement, the `temporaries` will go out of scope.

Besides, the awaiter itself has to be stored somewhere, by default, _CO2_ reserves `(sizeof(pointer) + sizeof(int)) * 2` bytes for that, if the size of awaiter is larger than that, dynamic allocation will be used.
If the default size is too large or too small for you, you can specify the desired size with `CO2_TEMP_SIZE` anywhere in the local variables section:
```c++
auto f() CO2_BEG(return_type, (),
    CO2_TEMP_SIZE(bytes);
)
{
    ...
} CO2_END
```

If you want to avoid dynamic allocation, you can define `CO2_WARN_DYN_ALLOC` to turn on dynamic allocation warning and enlarge `CO2_TEMP_SIZE` accordingly.

### Replacements for normal language constructs

Sometimes you can't use the normal language constructs directly, in such cases, you need to use the macro replacements instead.

#### return

* `return` -> `CO2_RETURN()`
* `return non-void-expr` -> `CO2_RETURN(non-void-expr)`
* `return maybe-void-expr` -> `CO2_RETURN_FROM(maybe-void-expr)` (useful in generic code)
* `return local-variable` -> `CO2_RETURN_LOCAL(local-variable)` (RV w/o explicit move)

#### try-catch

Needed only if the try-block is involved with the suspend-resume points.

```c++
CO2_TRY {...}
CO2_CATCH (std::runtime_error& e) {...}
catch (std::exception& e) {...}
```

Note that only the first `catch` clause needs to be spelled as `CO2_CATCH`, the subsequent ones should use the plain `catch`.

#### switch-case

Needed only if the switch-body is involved with the suspend-resume points. There are 2 variants:
* `CO2_SWITCH`
* `CO2_SWITCH_CONT` - use when switch-body contains `continue`.

```c++
CO2_SWITCH (which,
case 1,
(
    ...
),
case N,
(
    ...
),
default,
(
    ...
))
```

Note that `break` is still needed if you don't want the control flow to fall through the subsequent cases, also note that `continue` **cannot** be used in `CO2_SWITCH` to continue the outer loop, use `CO2_SWITCH_CONT` instead in that case.

## Difference from N4286

* Unlike `coroutine_handle` in N4286 which has raw-pointer semantic (i.e. no RAII), `coroutine` has unique-semantic (move-only).
* `coroutine_traits` depends on return_type only.

### Additional customization points for promise_type

* `void cancel()`

This allows you specify the behavior of the coroutine when it is cancelled (i.e. when `cancellation_requested()` returns true or coroutine is reset).

* `bool try_suspend()`

This is called before the coroutine is suspended, if it returns `false`, the coroutine won't be suspended, instead, it will be cancelled.
However, it won't be called for `final_suspend`.

* `bool try_resume()`

This is called before the coroutine is resumed, if it returns `false`, the coroutine won't be resumed, instead, it will be detached.

* `bool try_cancel()`

This is called before the coroutine is reset, if it returns `false`, the coroutine won't be cancelled, instead, it will be detached.

## Reference

__Headers__
* `#include <co2/coroutine.hpp>`
* `#include <co2/generator.hpp>`
* `#include <co2/recursive_generator.hpp>`
* `#include <co2/task.hpp>`
* `#include <co2/shared_task.hpp>`
* `#include <co2/lazy_task.hpp>`
* `#include <co2/sync/event.hpp>`
* `#include <co2/sync/mutex.hpp>`
* `#include <co2/sync/work_group.hpp>`
* `#include <co2/sync/when_all.hpp>`
* `#include <co2/sync/when_any.hpp>`
* `#include <co2/blocking.hpp>`
* `#include <co2/adapted/boost_future.hpp>`
* `#include <co2/adapted/boost_optional.hpp>`
* `#include <co2/utility/stack_allocator.hpp>`

__Macros__
* `CO2_BEG`
* `CO2_END`
* `CO2_AWAIT`
* `CO2_AWAIT_SET`
* `CO2_AWAIT_LET`
* `CO2_AWAIT_RETURN`
* `CO2_AWAIT_APPLY`
* `CO2_YIELD`
* `CO2_SUSPEND`
* `CO2_RETURN`
* `CO2_RETURN_FROM`
* `CO2_RETURN_LOCAL`
* `CO2_TRY`
* `CO2_CATCH`
* `CO2_SWITCH`
* `CO2_TEMP_SIZE`
* `CO2_AUTO`

__Classes__
* `co2::coroutine_traits<R>`
* `co2::coroutine<Promise>`
* `co2::generator<T>`
* `co2::recursive_generator<T>`
* `co2::task<T>`
* `co2::shared_task<T>`
* `co2::lazy_task<T>`
* `co2::event`
* `co2::mutex`
* `co2::work_group`
* `co2::suspend_always`
* `co2::suspend_never`
* `co2::stack_manager`
* `co2::stack_buffer<Bytes>`
* `co2::stack_allocator<T>`

## Example

### Generator

__Define a generator__
```c++
auto range(int i, int e) CO2_BEG(co2::generator<int>, (i, e))
{
    for ( ; i != e; ++i)
        CO2_YIELD(i);
} CO2_END
```
For those interested in the black magic, [here](https://gist.github.com/jamboree/d6c324b6cd4a11676cda) is the preprocessed output (formatted for reading).

__Use a generator__
```c++
for (auto i : range(1, 10))
{
    std::cout << i << ", ";
}
```

### Recursive Generator

Same example as above, using `recursive_generator` with custom allocator:
```c++
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
```
We use `stack_allocator` here:
```c++
co2::stack_buffer<64 * 1024> buf;
co2::stack_allocator<> alloc(buf);
for (auto i : recursive_range(alloc, 1, 10))
{
    std::cout << i << ", ";
}
```

### Task scheduling
It's very easy to write a generic task that can be used with different schedulers.
For example, a `fib` task that works with [`concurrency::task_group`](https://msdn.microsoft.com/en-us/library/dd470722.aspx) and [`tbb::task_group`](https://software.intel.com/en-us/node/506287) can be defined as below:
```c++
template<class Scheduler>
auto fib(Scheduler& sched, int n) CO2_BEG(co2::task<int>, (sched, n),
    co2::task<int> a, b;
)
{
    // Schedule the continuation.
    CO2_SUSPEND([&](co2::coroutine<>& c) { sched.run([h = c.detach()]{ co2::coroutine<>{h}(); }); });
    // From now on, the code is executed on the Scheduler.
    if (n >= 2)
    {
        a = fib(sched, n - 1);
        b = fib(sched, n - 2);
        CO2_AWAIT_SET(n, a);
        CO2_AWAIT_APPLY(n +=, b);
    }
    CO2_RETURN(n);
} CO2_END
```

#### PPL Usage
```c++
concurrency::task_group sched;
auto val = fib(sched, 16);
std::cout << "ans: " << co2::get(val);
sched.wait();
```

#### TBB Usage
```c++
tbb::task_group sched;
auto val = fib(sched, 16);
std::cout << "ans: " << co2::get(val);
sched.wait();
```

### ASIO echo server

This example uses the sister library [act](https://github.com/jamboree/act) to change ASIO style callback into await.

```c++
auto session(asio::ip::tcp::socket sock) CO2_BEG(void, (sock),
    char buf[1024];
    std::size_t len;
    act::error_code ec;
)
{
    CO2_TRY
    {
        std::cout << "connected: " << sock.remote_endpoint() << std::endl;
        for ( ; ; )
        {
            CO2_AWAIT_SET(len, act::read_some(sock, asio::buffer(buf), ec));
            if (ec == asio::error::eof)
                CO2_RETURN();
            CO2_AWAIT(act::write(sock, asio::buffer(buf, len)));
        }
    }
    CO2_CATCH (std::exception& e)
    {
        std::cout << "error: " << sock.remote_endpoint() << ": " << e.what() << std::endl;
    }
} CO2_END

auto server(asio::io_service& io, unsigned short port) CO2_BEG(void, (io, port),
    asio::ip::tcp::endpoint endpoint{asio::ip::tcp::v4(), port};
    asio::ip::tcp::acceptor acceptor{io, endpoint};
    asio::ip::tcp::socket sock{io};
)
{
    std::cout << "server running at: " << endpoint << std::endl;
    for ( ; ; )
    {
        CO2_AWAIT(act::accept(acceptor, sock));
        session(std::move(sock));
    }
} CO2_END
```

## Performance
The overhead of context-switch. See [benchmark.cpp](test/benchmark.cpp).

Sample run (VS2015 Update 3, boost 1.63.0, 64-bit release build):
```
Run on (4 X 3200 MHz CPU s)
Benchmark                  Time           CPU Iterations
--------------------------------------------------------
bench_coroutine2         82 ns         80 ns    8960000
bench_co2                  6 ns          6 ns  112000000
bench_msvc                 5 ns          5 ns  112000000
```
Lower is better.

![benchmark](doc/benchmark.png?raw=true)

## License

    Copyright (c) 2015-2018 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

<!-- Links -->
[badge.Wandbox]: https://img.shields.io/badge/try%20it-online-green.svg
