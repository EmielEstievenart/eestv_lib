#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>
#include "eestv/serial/allowed_types.hpp"
#include "eestv/serial/serialize_helper.hpp"
#include "eestv/data/linear_buffer.hpp"

namespace eestv
{

/**
 * @brief Serializer that writes data directly to a LinearBuffer
 * 
 * Uses get_write_head() and commit() for efficient zero-copy serialization.
 */
class Serializer
{
public:
    /**
     * @brief Construct a new Serializer
     * 
     * @param buffer Reference to the LinearBuffer
     */
    explicit Serializer(LinearBuffer& buffer) : _buffer(buffer), _bytes_written(0) { }

    /**
     * @brief Serialize a value using operator&
     * 
     * This operator enables the boost::serialization-style syntax:
     * ar & value1 & value2 & value3;
     * 
     * @tparam T The type to serialize
     * @param value The value to serialize
     * @return Serializer& Reference to this serializer for chaining
     */
    template <typename T>
    Serializer& operator&(const T& value)
    {
        check_if_type_is_serializable<T>();
        return serialize_value(value);
    }

    /**
     * @brief Get the number of bytes written so far
     * 
     * @return std::size_t Number of bytes written
     */
    std::size_t bytes_written() const { return _bytes_written; }

    /**
     * @brief Reset the byte counter
     */
    void reset() { _bytes_written = 0; }

private:
    LinearBuffer& _buffer;
    std::size_t _bytes_written;

    /**
     * @brief Serialize a primitive type
     * 
     * @tparam T The primitive type
     * @param value The value to serialize
     * @return Serializer& Reference to this serializer
     */
    template <typename T>
    typename std::enable_if<std::is_arithmetic<T>::value, Serializer&>::type serialize_value(const T& value)
    {
        std::size_t available;
        std::uint8_t* write_head = _buffer.get_write_head(available);

        if (write_head != nullptr && available >= sizeof(T))
        {
            std::memcpy(write_head, &value, sizeof(T));
            if (_buffer.commit(sizeof(T)))
            {
                _bytes_written += sizeof(T);
            }
        }
        return *this;
    }

    /**
     * @brief Serialize a user-defined type with serialize() method or free function
     * 
     * @tparam T The user-defined type
     * @param value The value to serialize
     * @return Serializer& Reference to this serializer
     */
    template <typename T>
    typename std::enable_if<!std::is_arithmetic<T>::value, Serializer&>::type serialize_value(const T& value)
    {
        // Main dispatcher: prefer member, otherwise ADL free function, otherwise static assert
        if constexpr (has_member_serialize<T, Serializer>::value)
        {
            // Member exists: call it
            const_cast<T&>(value).serialize(*this);
        }
        else if constexpr (has_adl_serialize<T, Serializer>::value)
        {
            // No member, but ADL-visible free function exists: unqualified call enables ADL
            serialize(const_cast<T&>(value), *this);
        }
        else
        {
            static_assert(always_false<T>::value, "Type T must provide either a member function 'void serialize(Archive&)' or a free "
                                                  "function 'void serialize(T&, Archive&)'");
        }
        return *this;
    }
};

} // namespace eestv
