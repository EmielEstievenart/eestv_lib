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
     * @brief Get pointer to the read head (data ready to be read/sent) and number of readable bytes
     * 
     * @param size_out Output parameter that will contain the number of bytes available to read
     * @return const std::uint8_t* Pointer to the start of readable data, or nullptr if empty
     */
    const std::uint8_t* get_read_head(std::size_t& size_out) const;

    /**
     * @brief Remove/consume the specified number of bytes from the front of the buffer
     * 
     * @param size Number of bytes to consume
     * @return true if bytes were successfully consumed, false if not enough data available
     */
    bool consume(std::size_t size);

    /**
     * @brief Get pointer to the write head (where new data can be written) and number of writable bytes
     * 
     * @param size_out Output parameter that will contain the number of bytes available to write
     * @return std::uint8_t* Pointer to the location where new data can be written, or nullptr if no space
     */
    std::uint8_t* get_write_head(std::size_t& size_out);

    /**
     * @brief Commit written bytes to the buffer
     * 
     * Call this after data has been written directly to the buffer
     * (e.g., after async_read_some completes) to advance the write position.
     * 
     * @param bytes_written Number of bytes that were written
     * @return true if successful, false if size exceeds available space
     */
    bool commit(std::size_t bytes_written);

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
