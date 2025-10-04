#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * @brief A linear buffer that allows efficient sequential data access without copying.
 * 
 * Unlike a circular buffer, when the buffer becomes empty, both head and tail reset to 0,
 * ensuring that data is always stored contiguously and can be accessed via direct pointers
 * without requiring data copying across buffer boundaries.
 */
class LinearBuffer
{
public:
    /**
     * @brief Construct a new Linear Buffer object
     * 
     * @param size The maximum capacity of the buffer in bytes
     */
    explicit LinearBuffer(std::size_t size);

    /**
     * @brief Push data into the buffer
     * 
     * @param data Pointer to the data to be pushed
     * @param size Number of bytes to push
     * @return true if data was successfully pushed, false if insufficient space
     */
    bool push(const void* data, std::size_t size);

    /**
     * @brief Get a pointer to the next available data and its size
     * 
     * @param size_out Output parameter that will contain the size of available data
     * @return const void* Pointer to the start of the next data chunk, or nullptr if empty
     */
    const void* peek(std::size_t& size_out) const;

    /**
     * @brief Remove/consume the specified number of bytes from the front of the buffer
     * 
     * @param size Number of bytes to consume
     * @return true if bytes were successfully consumed, false if not enough data available
     */
    bool consume(std::size_t size);

    /**
     * @brief Get the number of bytes currently stored in the buffer
     * 
     * @return std::size_t Number of bytes of data available
     */
    std::size_t available_data() const;

    /**
     * @brief Get the number of bytes of free space in the buffer
     * 
     * @return std::size_t Number of bytes available for writing
     */
    std::size_t available_space() const;

    /**
     * @brief Get the total capacity of the buffer
     * 
     * @return std::size_t Total buffer capacity in bytes
     */
    std::size_t capacity() const;

    /**
     * @brief Check if the buffer is empty
     * 
     * @return true if buffer is empty, false otherwise
     */
    bool is_empty() const;

    /**
     * @brief Check if the buffer is full
     * 
     * @return true if buffer is full, false otherwise
     */
    bool is_full() const;

    /**
     * @brief Clear all data from the buffer and reset head/tail to 0
     */
    void clear();

private:
    std::vector<std::uint8_t> _buffer;
    std::size_t _head; // Index where next data will be written
    std::size_t _tail; // Index where next data will be read from
    std::size_t _size; // Current amount of data in buffer

    /**
     * @brief Reset head and tail to 0 when buffer becomes empty
     */
    void reset_if_empty();
};
