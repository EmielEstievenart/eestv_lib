#pragma once

#include <type_traits>
#include <utility>

namespace eestv
{

// Trait: has a member function `serialize(Archive&)` callable on an lvalue T
template <typename T, typename S, typename = void>
struct has_member_serialize : std::false_type
{
};

template <typename T, typename S>
struct has_member_serialize<T, S, std::void_t<decltype(std::declval<T&>().serialize(std::declval<S&>()))> > : std::true_type
{
};

// Trait: has a free function `serialize(T&, Archive&)` found by ADL (unqualified call)
template <typename, typename S, typename = void>
struct has_adl_serialize : std::false_type
{
};

template <typename T, typename S>
struct has_adl_serialize<T, S, std::void_t<decltype(serialize(std::declval<T&>(), std::declval<S&>()))> > : std::true_type
{
};

// Helper to produce better static_assert messages
template <typename T>
struct always_false : std::false_type
{
};

}
