CO2 - Coroutine II
===

A C++ stackless coroutine emulation library, providing interface close to [N4286](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4286.pdf).

## Requirements

- C++14
- [Boost](http://www.boost.org/)

## Motivation

I like the idea of `await` as proposed in N4286, and I hope it will become part of the new standard (C++17?), but as of this writing, there's no working implementation available (MSVC14? no, it's broken), so I started to work out a preprocessor-based library to emulate `await` and stackless coroutine.

## Overview

Many of the concepts are similar to N4286, if you're not familiar with the proposal, please read the paper first.

A coroutine written in this library looks like below:
```c++
return_type function(Args... args)
CO2_BEGIN(return_type, (args...), locals...)
{
    <coroutine-body>
}
CO2_END
```

Note this line
```c++
return_type function(Args... args)
```
is just a plain-old function prototype, you can forward declare it as well.

Ready for preprocessor magic? here we go...

The coroutine body is surrounded with 2 macros: `CO2_BEGIN` and `CO2_END`.

The macro `CO2_BEGIN` requires you to provide some parameters:
* _return_type_ - same as the function's return-type
* _args_ - captures section, a list of comma (`,`) separated identifiers
* _locals_ - local variables section, a list of semi-colon (`;`) separated variables

Both _args_ and _locals_ are optional, depends on your need, you can leave them empty, for example:
```c++
CO2_BEGIN(return_type, ())
```

You may find that repeating the return_type twice is annoying, as C++11 adds trailing return type syntax, the library also provide a convenient macro `CO2_RET`, which is particularly fit for lambda expressions:
```c++
[]() CO2_RET(return_type, ()) {...} CO2_END
```

You can intialize the local variables in the local variables section, for example:
```c++
auto f(int i) CO2_RET(return_type, (i),
    int i2 = i * 2;
)
{
    // coroutine-body
} CO2_END
```

Note that in this emulation, local variables intialization happens before `initial_suspend`, and if any exception is thrown during the intialization, `set_exception` won't be called, instead, the exception will propagate to the caller directly.

Inside the coroutine body, there are some restrictions:
* local variables with automatic storage are not allowed - you should specify them in local variables section of `CO2_BEGIN` as described above
* `return` should be replaced with `CO2_RETURN`/`CO2_RETURN_FROM`
* try-catch block surrouding `await` statements should be replaced with `CO2_TRY` & `CO2_CATCH`
* identifiers starting with `_co2_` are reserved for this library

### await & yield

In _CO2_, `await` is implemented as a statement instead of an expression due to the emulation limitation, and it has 4 variants: `CO2_AWAIT`, `CO2_AWAIT_SET`, `CO2_AWAIT_LET` and `CO2_AWAIT_RETURN`.

* `CO2_AWAIT(expr)`

Equivalent to `await expr`.

* `CO2_AWAIT_SET(var, expr)`

Equivalent to `var = await expr`.

* `CO2_AWAIT_LET(var-decl, expr, body)`

This is to eliminate the need of a local variable, for example:
```c++
CO2_AWAIT_LET(auto i, task,
{
    doSomething(i);
});
```

* `CO2_AWAIT_RETURN(expr)`

Equivalent to `return await expr`.

As `yield` is defined in N4286, _CO2_ also provides the corresponding `CO2_YIELD`.
`CO2_YIELD(expr)` is equivalent to `CO2_AWAIT(<this-promise>.yield_value(expr))`.

The fact that `await` in _CO2_ is not an expression has an implication on object lifetime, consider this case:

`await something{temporaries}` and `something` holds references to temporaries.

It's safe if `await` is an expression as in N4286, but in _CO2_, `CO2_AWAIT(something{temporaries})` is an emulated statement, the `temporaries` will go out of scope.

Besides, the awaiter itself has to be stored somewhere, _CO2_ internally reserves `sizeof(pointer) * 4` bytes for that as default, if the size of awaiter is larger than that, free store will be used.
If the default size is too large or too small for you, you can specify the desired size with `CO2_RESERVE` anywhere in the local variables section:
```c++
auto f() CO2_RET(return_type, (),
    CO2_RESERVE(bytes);
)
{
    ...
} CO2_END
```

### Replacements for normal language constructs

Sometimes you can't use the normal language constructs directly, in such cases, you need to use the macro replacements instead.

#### return

* `return` -> `CO2_RETURN()`
* `return expr` -> `CO2_RETURN(expr)`
* `return void-expr` -> `CO2_RETURN_FROM(void-expr)` (useful in generic code)

#### try-catch

Needed only if the try-block is involved with the await/yield emulation.

```c++
CO2_TRY {...}
CO2_CATCH (std::runtime_error& e) {...}
catch (std::exception& e) {...}
```

Note that only the first `catch` clause needs to be spelled as `CO2_CATCH`, the subsequent ones should use the plain `catch`.

#### switch-case

Needed only if the switch-body is involved with the await/yield emulation.

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

Note that `break` is still needed if you don't want the control flow to go through the subsequnet cases, also note that `continue` **cannot** be used in `CO2_SWITCH` to continue the outer loop due to some implementation details.

## Difference from N4286

* Unlike `coroutine_handle` in N4286, `coroutine` is ref-counted.
* `coroutine_traits` depends on return_type only.
* `final_suspend` is replaced with `finalize` in promise requirements, always returns `void`.

### Additional customization points

In addition to the promise requirements defined by N4286, _CO2_ defines an optional pair of customization points:
1. `bool before_resume()`
2. `void after_suspend()`

`before_resume` is called for checking if the coroutine can be resumed, and `after_suspend` is called after `await_suspend` succeed on the awaiter.
Combined with `cancellation_requested`, they allow you to actively cancel the coroutine instead of passively waiting for the awaiter to resume-and-cancel.

## Reference

__Headers__
* `#include <co2/coroutine.hpp>`
* `#include <co2/generator.hpp>`
* `#include <co2/recursive_generator.hpp>`
* `#include <co2/task.hpp>`
* `#include <co2/shared_task.hpp>`
* `#include <co2/adapted/boost_future.hpp>`
* `#include <co2/utility/stack_allocator.hpp>`

__Macros__
* `CO2_BEGIN`
* `CO2_RET`
* `CO2_END`
* `CO2_AWAIT`
* `CO2_AWAIT_SET`
* `CO2_AWAIT_LET`
* `CO2_AWAIT_RETURN`
* `CO2_YIELD`
* `CO2_RETURN`
* `CO2_RETURN_FROM`
* `CO2_TRY`
* `CO2_CATCH`
* `CO2_SWITCH`
* `CO2_RESERVE`

__Classes__
* `co2::coroutine_traits<R>`
* `co2::coroutine<Promise>`
* `co2::generator<T>`
* `co2::recursive_generator<T>`
* `co2::task<T>`
* `co2::shared_task<T>`
* `co2::suspend_always`
* `co2::suspend_never`
* `co2::stack_manager`
* `co2::stack_buffer<Bytes>`
* `co2::stack_allocator<T>`

## Example

### Generator

__Define a generator__
```c++
auto range(int i, int e) CO2_RET(co2::generator<int>, (i, e))
{
    for ( ; i != e; ++i)
        CO2_YIELD(i);
} CO2_END
```

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
auto recursive_range(std::allocator_arg_t use_alloc, Alloc alloc, int a, int b)
CO2_RET(co2::recursive_generator<int>, (use_alloc, alloc, a, b),
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
    CO2_YIELD(recursive_range(use_alloc, alloc, a, n));
    CO2_YIELD(recursive_range(use_alloc, alloc, n, b));
} CO2_END
```
We use `stack_allocator` here:
```c++
std::allocator_arg_t use_alloc;
co2::stack_buffer<64 * 1024> buf;
co2::stack_allocator<> alloc(buf);
for (auto i : recursive_range(use_alloc, alloc, 1, 10))
{
    std::cout << i << ", ";
}
```

### ASIO echo server

This example uses the sister library [act](https://github.com/jamboree/act) to change ASIO style callback into await.

```c++
auto session(asio::ip::tcp::socket sock) CO2_RET(co2::task<>, (sock),
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

auto server(asio::io_service& io, unsigned port) CO2_RET(co2::task<>, (io, port),
    asio::ip::tcp::endpoint endpoint{asio::ip::tcp::v4(), port};
    asio::ip::tcp::acceptor acceptor{io, endpoint};
)
{
    std::cout << "server running at: " << endpoint << std::endl;
    for ( ; ; )
        CO2_AWAIT_LET(auto&& sock, act::accept(acceptor),
        {
            session(std::move(sock));
        });
} CO2_END
```

## License

    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
