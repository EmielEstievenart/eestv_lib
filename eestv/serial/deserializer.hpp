#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include "eestv/serial/allowed_types.hpp"
#include "eestv/serial/serialize_helper.hpp"

namespace eestv
{

/**
 * @brief Deserializer that reads data from a storage policy
 * 
 * The Storage type must provide:
 * - bool read(void* data, size_t size)
 * 
 * @tparam Storage The storage policy type (e.g., LinearBufferAdapter, VectorAdapter)
 */
template <typename Storage>
class Deserializer
{
public:
    /**
     * @brief Construct a new Deserializer
     * 
     * @param storage Reference to the storage backend
     */
    explicit Deserializer(Storage& storage) : _storage(storage), _bytes_read(0) { }

    /**
     * @brief Deserialize a value using operator&
     * 
     * This operator enables the boost::serialization-style syntax:
     * ar & value1 & value2 & value3;
     * 
     * @tparam T The type to deserialize
     * @param value Reference to the value to deserialize into
     * @return Deserializer& Reference to this deserializer for chaining
     */
    template <typename T>
    Deserializer& operator&(T& value)
    {
        check_if_type_is_serializable<T>();
        return deserialize_value(value);
    }

    /**
     * @brief Get the number of bytes read so far
     * 
     * @return std::size_t Number of bytes read
     */
    std::size_t bytes_read() const { return _bytes_read; }

    /**
     * @brief Reset the byte counter
     */
    void reset() { _bytes_read = 0; }

private:
    Storage& _storage;
    std::size_t _bytes_read;

    /**
     * @brief Deserialize a primitive type
     * 
     * @tparam T The primitive type
     * @param value Reference to the value to deserialize into
     * @return Deserializer& Reference to this deserializer
     */
    template <typename T>
    typename std::enable_if<std::is_arithmetic<T>::value, Deserializer&>::type deserialize_value(T& value)
    {
        if (_storage.read(&value, sizeof(T)))
        {
            _bytes_read += sizeof(T);
        }
        return *this;
    }

    /**
     * @brief Deserialize a user-defined type with serialize() method or free function
     * 
     * @tparam T The user-defined type
     * @param value Reference to the value to deserialize into
     * @return Deserializer& Reference to this deserializer
     */
    template <typename T>
    typename std::enable_if<!std::is_arithmetic<T>::value, Deserializer&>::type deserialize_value(T& value)
    {
        // Main dispatcher: prefer member, otherwise ADL free function, otherwise static assert
        if constexpr (has_member_serialize<T, Deserializer>::value)
        {
            // Member exists: call it
            value.serialize(*this);
        }
        else if constexpr (has_adl_serialize<T, Deserializer>::value)
        {
            // No member, but ADL-visible free function exists: unqualified call enables ADL
            serialize(value, *this);
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
