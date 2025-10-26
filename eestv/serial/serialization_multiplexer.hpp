#pragma once

#include "eestv/serial/serializer.hpp"
#include "eestv/serial/serialization_mux_common.hpp"
#include "eestv/data/linear_buffer.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace eestv
{

/**
 * @brief Serialization multiplexer that can serialize multiple types with metadata
 * 
 * Format: [1 byte: type_index] [2 bytes: payload_size] [N bytes: serialized_payload]
 * 
 * Usage:
 * @code
 * SerializationMultiplexer<TypeA, TypeB, TypeC> mux(buffer);
 * mux.serialize(instance_of_type_a);
 * mux.serialize(instance_of_type_b);
 * @endcode
 * 
 * @tparam Types The types that can be serialized through this multiplexer
 */
template <typename... Types>
class SerializationMultiplexer
{
    static_assert(sizeof...(Types) > 0, "At least one type must be specified");
    static_assert(sizeof...(Types) <= max_types, "Maximum 255 types supported (1 byte index)");

public:
    /**
     * @brief Construct a new SerializationMultiplexer
     * 
     * @param buffer Reference to the LinearBuffer
     */
    explicit SerializationMultiplexer(LinearBuffer& buffer) : _buffer(buffer) { }

    /**
     * @brief Serialize an object with type metadata
     * 
     * Single-pass approach with size patching:
     * 1. Write header with type index and placeholder size (0x00, 0x00)
     * 2. Commit header to advance write position
     * 3. Serialize payload directly to buffer
     * 4. Patch the size bytes using the saved pointer (still valid for direct writes)
     * 
     * @tparam T The type to serialize (must be one of Types...)
     * @param value The value to serialize
     * @return true if serialization succeeded, false otherwise
     */
    template <typename T>
    bool serialize(const T& value)
    {
        static_assert((std::is_same_v<T, Types> || ...), "Type T must be one of the registered types");

        // Get type index
        constexpr std::uint8_t type_index = static_cast<std::uint8_t>(type_index_impl<T, Types...>::value);

        // Get write pointer and check space for at least the header
        std::size_t available    = 0;
        std::uint8_t* write_head = _buffer.get_write_head(available);

        if (write_head == nullptr || available < header_size)
        {
            return false;
        }

        // Write header with type index and placeholder size (will be patched later)
        write_head[0] = type_index;
        write_head[1] = 0x00; // Placeholder for size low byte
        write_head[2] = 0x00; // Placeholder for size high byte

        // Commit header
        if (!_buffer.commit(header_size))
        {
            return false;
        }

        // Serialize payload directly to buffer
        Serializer ser(_buffer);
        ser & value;
        std::size_t payload_size = ser.bytes_written();

        // Check size fits in 2 bytes (uint16_t max = 65535)
        if (payload_size > max_payload_size)
        {
            // Size too large - this is an error condition
            return false;
        }

        // Patch the size field using the saved pointer (still valid for direct memory writes)
        write_head[1] = static_cast<std::uint8_t>(payload_size & byte_mask);                    // Low byte
        write_head[2] = static_cast<std::uint8_t>((payload_size >> bits_per_byte) & byte_mask); // High byte

        return true;
    }

private:
    LinearBuffer& _buffer;
};

} // namespace eestv
