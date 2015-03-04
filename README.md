CO2 - Coroutine The Second
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
    coroutine_body
}
CO2_END
```

Note this line
```c++
return_type function(Args... args)
```
is just a plain-old function prototype, you can forward declare it as well.

Ready preprocessor magic? Here we go ;)

The coroutine body is surrounded with 2 macros: `CO2_BEGIN` and `CO2_END`.

The macro `CO2_BEGIN` requires you to provide some parameters:
* _return_type_ - same as the function's return-type
* _args_ - captures section, a list of comma (`,`) separated identifiers
* _locals_ - local variables section, a list of semi-colon (`;`) separated variables

Both _args_ and _locals_ are optional, depends on your need, you can leave them empty, for example:
```c++
CO2_BEGIN(return_type, ())
```

Inside the coroutine body, there are some restrictions:
* auto local variables are not allowed - you should specify them in local variables section of `CO2_BEGIN`
* `return`, try-catch should be replace with the corresponding macros

Besides, `await` is implemented as a statement instead of an expression due to the emulation restriction, and it has 3 variants: `CO2_AWAIT`, `CO2_AWAIT_GET` and `CO2_AWAIT_LET`.

* `CO2_AWAIT(expr)`

Equivalent to `await expr`, for example:
```c++
CO2_AWAIT(task);
```
* `CO2_AWAIT_GET(var, expr)`

Equivalent to `var = await expr`, for example:
```c++
CO2_AWAIT_GET(i, task);
```

* `CO2_AWAIT_LET(var-decl, expr, body)`

This is to eliminate the need of a local variable, for example:
```c++
CO2_AWAIT_LET(auto i, task,
{
    doSomething(i);
});
```

__Macros__
* `CO2_BEGIN`
* `CO2_END`
* `CO2_AWAIT`
* `CO2_AWAIT_GET`
* `CO2_AWAIT_LET`
* `CO2_RETURN`
* `CO2_TRY`
* `CO2_CATCH`

__Classes__
* `coroutine`
* `suspend_always`
* `suspend_never`

## Tutorial

## Example

TODO

## License

    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)