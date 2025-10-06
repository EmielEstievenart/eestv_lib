#include "eestv/data/linear_buffer.hpp"
#include <cstring>

LinearBuffer::LinearBuffer(std::size_t size) : _buffer(size), _head(0), _tail(0), _size(0)
{
}

const std::uint8_t* LinearBuffer::get_read_head(std::size_t& size_out) const
{
    size_out = _size;
    return _size > 0 ? &_buffer[_tail] : nullptr;
}

bool LinearBuffer::consume(std::size_t size)
{
    if (size > _size)
    {
        return false;
    }

    _tail += size;
    _size -= size;

    reset_if_empty();

    return true;
}

std::uint8_t* LinearBuffer::get_write_head(std::size_t& size_out)
{
    // Available space is the total capacity minus current size,
    // but we can only write up to the end of the buffer from current head position
    std::size_t space_to_end = _buffer.size() - _head;
    std::size_t total_free   = _buffer.size() - _size;
    size_out                 = (space_to_end < total_free) ? space_to_end : total_free;
    return (size_out > 0) ? &_buffer[_head] : nullptr;
}

bool LinearBuffer::commit(std::size_t bytes_written)
{
    std::size_t writable;
    get_write_head(writable);

    if (bytes_written > writable)
    {
        return false;
    }

    _head += bytes_written;
    _size += bytes_written;
    return true;
}

void LinearBuffer::reset_if_empty()
{
    if (_size == 0)
    {
        _head = 0;
        _tail = 0;
    }
}
