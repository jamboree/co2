CO2 - Coroutine II
===

A C++ stackless coroutine emulation library, providing interface close to [N4286](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4286.pdf).

## Requirements

- C++11
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

Inside the coroutine body, there are some restrictions:
* auto local variables are not allowed - you should specify them in local variables section of `CO2_BEGIN`
* `return`, try-catch should be replace with the corresponding macros
* identifiers starting with `_co2_` are reserved for this library

__return statement__
* `return` -> `CO2_RETURN()`
* `return expr` -> `CO2_RETURN(expr)`

__try-catch__
```c++
CO2_TRY {...}
CO2_CATCH (std::runtime_error& e) {...}
catch (std::exception& e) {...}
```

Note that only the first `catch` clause needs to be spelled as `CO2_CATCH`, the subsequent ones should use the plain `catch`.

### await & yield

In _CO2_, `await` is implemented as a statement instead of an expression due to the emulation restriction, and it has 3 variants: `CO2_AWAIT`, `CO2_AWAIT_SET` and `CO2_AWAIT_LET`.

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

As `yield` is defined in N4286, _CO2_ also provides the corresponding `CO2_YIELD`.
`CO2_YIELD(expr)` is equivalent to `CO2_AWAIT(<this-promise>.yield_value(expr))`.

The fact that `await` in _CO2_ is not an expression has an implication on object lifetime, consider this case:

`await something{temporaries}` and `something` holds references to temporaries.

It's safe if `await` is an expression as in N4286, but in _CO2_, `CO2_AWAIT(something{temporaries})` is an emulated statement, the temporaries will go out of scope.

## Difference from N4286

* Unlike `coroutine_handle` in N4286, `coroutine` is ref-counted.
* No `coroutine_traits`, _CO2_ always use `return_type::promise_type` for the promise.
* `promise_type::final_suspend` is ignored.

## Reference

__Headers__
* `#include <co2/coroutine.hpp>`

__Macros__
* `CO2_BEGIN`
* `CO2_RET`
* `CO2_END`
* `CO2_AWAIT`
* `CO2_AWAIT_SET`
* `CO2_AWAIT_LET`
* `CO2_YIELD`
* `CO2_RETURN`
* `CO2_TRY`
* `CO2_CATCH`

__Classes__
* `co2::coroutine<Promise>`
* `co2::generator<T>`
* `co2::suspend_always`
* `co2::suspend_never`

## Example

### Generator

__Define a generator__
```c++
auto range(int i, int e) CO2_RET(co2::generator<int>, (i, e))
{
    for (; i != e; ++i)
        CO2_YIELD(i);
} CO2_END;
```

__Use a generator__
```c++
for (auto i : range(1, 10))
{
    std::cout << i << ", ";
}
```

## License

    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)