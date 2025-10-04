#pragma once

#include <string>
#include <type_traits>

namespace eestv
{

template <typename T>
constexpr void check_if_type_is_serializable()
{
    static_assert(!std::is_same<T, float>::value, "Float isn't allowed");
    static_assert(!std::is_same<T, double>::value, "Double isn't allowed");
    static_assert(!std::is_same<T, long double>::value, "Long double isn't allowed");
}

};
