#pragma once

#include "eestv/serial/deserializer.hpp"
#include "eestv/serial/serialization_mux_common.hpp"
#include "eestv/data/linear_buffer.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <tuple>
#include <variant>

namespace eestv
{

/**
 * @brief Deserialization demultiplexer that can deserialize multiple types with metadata
 * 
 * Format: [1 byte: type_index] [2 bytes: payload_size] [N bytes: serialized_payload]
 * 
 * Usage:
 * @code
 * SerializationDemultiplexer<TypeA, TypeB, TypeC> demux(buffer);
 * auto variant = demux.deserialize(); // Returns std::variant<TypeA, TypeB, TypeC>
 * std::visit([](auto& obj) { // process obj }, variant);
 * @endcode
 * 
 * @tparam Types The types that can be deserialized through this demultiplexer
 */
template <typename... Types>
class SerializationDemultiplexer
{
    static_assert(sizeof...(Types) > 0, "At least one type must be specified");
    static_assert(sizeof...(Types) <= max_types, "Maximum 255 types supported (1 byte index)");

public:
    using variant_type = std::variant<Types...>;

    /**
     * @brief Construct a new SerializationDemultiplexer
     * 
     * @param buffer Reference to the LinearBuffer
     */
    explicit SerializationDemultiplexer(LinearBuffer& buffer) : _buffer(buffer) { }

    /**
     * @brief Deserialize an object, returning a variant containing the correct type
     * 
     * @param success_out Output parameter indicating if deserialization succeeded
     * @return variant_type The deserialized object in a variant
     */
    variant_type deserialize(bool& success_out)
    {
        // Read header
        std::uint8_t type_index    = 0;
        std::uint16_t payload_size = 0;

        if (!read_header(type_index, payload_size))
        {
            success_out = false;
            return variant_type {};
        }

        // Validate type index
        if (type_index >= sizeof...(Types))
        {
            success_out = false;
            return variant_type {};
        }

        // Deserialize based on type index
        variant_type result = deserialize_by_index(type_index, success_out);
        return result;
    }

private:
    LinearBuffer& _buffer;

    /**
     * @brief Read the header (type index and payload size)
     * 
     * @param type_index_out Output parameter for type index
     * @param payload_size_out Output parameter for payload size
     * @return true if header was read successfully
     */
    bool read_header(std::uint8_t& type_index_out, std::uint16_t& payload_size_out)
    {
        std::size_t available         = 0;
        const std::uint8_t* read_head = _buffer.get_read_head(available);

        if (read_head == nullptr || available < header_size)
        {
            return false;
        }

        std::array<std::uint8_t, header_size> header = {};
        std::memcpy(header.data(), read_head, header_size);
        _buffer.consume(header_size);

        type_index_out   = header[0];
        payload_size_out = static_cast<std::uint16_t>(header[1]) | (static_cast<std::uint16_t>(header[2]) << bits_per_byte);
        return true;
    }

    /**
     * @brief Deserialize based on runtime type index
     * 
     * @param type_index The type index from the header
     * @param success_out Output parameter indicating success
     * @return variant_type The deserialized object
     */
    variant_type deserialize_by_index(std::uint8_t type_index, bool& success_out)
    {
        variant_type result;
        success_out = deserialize_by_index_impl(type_index, result, std::index_sequence_for<Types...> {});
        return result;
    }

    /**
     * @brief Implementation of type-indexed deserialization using index sequence
     * 
     * @tparam Indices Index sequence for the types
     * @param type_index The runtime type index
     * @param result_out The variant to store the result in
     * @param indices_seq Index sequence (compile-time parameter)
     * @return true if deserialization succeeded
     */
    template <std::size_t... Indices>
    bool deserialize_by_index_impl(std::uint8_t type_index, variant_type& result_out, std::index_sequence<Indices...> /* indices_seq */)
    {
        // Use fold expression to try each index
        bool success = false;
        (void)((type_index == Indices ? (success = deserialize_type<std::tuple_element_t<Indices, std::tuple<Types...>>>(result_out), true)
                                      : false) ||
               ...);
        return success;
    }

    /**
     * @brief Deserialize a specific type
     * 
     * @tparam T The type to deserialize
     * @param result_out The variant to store the result in
     * @return true if deserialization succeeded
     */
    template <typename T>
    bool deserialize_type(variant_type& result_out)
    {
        T value {};
        Deserializer deser(_buffer);
        deser & value;

        result_out = std::move(value);
        return true;
    }
};

} // namespace eestv
