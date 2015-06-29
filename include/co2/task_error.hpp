/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2015 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef CO2_TASK_ERROR_HPP_INCLUDED
#define CO2_TASK_ERROR_HPP_INCLUDED

#include <stdexcept>

namespace co2
{
    enum class task_errc
    {
        cancelled = 1,
        no_state
    };
}

namespace std
{
    template<>
    struct is_error_code_enum<co2::task_errc> : true_type {};
}

namespace co2 { namespace task_detail
{
    inline char const* error_message(int condition)
    {
        switch (condition)
        {
        case task_errc::cancelled: return "cancelled";
        case task_errc::no_state: return "no_state";
        default: return "unknow error";
        }
    }

    struct error_category : std::error_category
    {
        char const *name() const noexcept override
        {
            return "task";
        }

        std::string message(int condition) const override
        {
            return error_message(condition);
        }
    };
}}

namespace co2
{
    inline std::error_category const& task_category()
    {
        static task_detail::error_category ec;
        return ec;
    }

    inline std::error_code make_error_code(task_errc ec) noexcept
    {
        return std::error_code(static_cast<int>(ec), task_category());
    }

    struct task_error : std::logic_error
    {
        task_error(std::error_code ec) : logic_error(""), _ec(ec) {}

        std::error_code const& code() const noexcept
        {
            return _ec;
        }

        char const* what() const noexcept override
        {
            return task_detail::error_message(_ec.value());
        }

    private:

        std::error_code _ec;
    };
}

#endif