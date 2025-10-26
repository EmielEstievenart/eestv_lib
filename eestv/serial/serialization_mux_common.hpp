#pragma once

#include "eestv/data/linear_buffer.hpp"
#include <cstddef>
#include <cstdint>

namespace eestv
{

// Constants for multiplexer protocol
constexpr std::size_t max_types          = 255;
constexpr std::size_t header_size        = 3;
constexpr std::uint16_t max_payload_size = 0xFFFF;
constexpr std::uint8_t byte_mask         = 0xFF;
constexpr std::size_t bits_per_byte      = 8;

/**
 * @brief Helper to find the index of a type in a parameter pack
 * 
 * @tparam T The type to find
 * @tparam Types The parameter pack to search in
 */
template <typename T, typename... Types>
struct type_index_impl;

template <typename T, typename... Rest>
struct type_index_impl<T, T, Rest...>
{
    static constexpr std::size_t value = 0;
};

template <typename T, typename First, typename... Rest>
struct type_index_impl<T, First, Rest...>
{
    static constexpr std::size_t value = 1 + type_index_impl<T, Rest...>::value;
};

} // namespace eestv
