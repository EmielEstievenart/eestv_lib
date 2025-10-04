#include "eestv/data/linear_buffer.hpp"
#include <cstring>

LinearBuffer::LinearBuffer(std::size_t size) 
    : _buffer(size), _head(0), _tail(0), _size(0)
{
}

bool LinearBuffer::push(const void* data, std::size_t size)
{
    if (size == 0 || data == nullptr)
    {
        return false;
    }

    // Check if we have enough space
    if (available_space() < size)
    {
        return false;
    }

    // Check if we have enough contiguous space at the current head position
    if (_head + size > _buffer.size())
    {
        return false;
    }

    // Copy data to buffer
    std::memcpy(&_buffer[_head], data, size);
    _head += size;
    _size += size;

    return true;
}

const void* LinearBuffer::peek(std::size_t& size_out) const
{
    if (_size == 0)
    {
        size_out = 0;
        return nullptr;
    }

    size_out = _size;
    return &_buffer[_tail];
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

std::size_t LinearBuffer::available_data() const
{
    return _size;
}

std::size_t LinearBuffer::available_space() const
{
    return _buffer.size() - _size;
}

std::size_t LinearBuffer::capacity() const
{
    return _buffer.size();
}

bool LinearBuffer::is_empty() const
{
    return _size == 0;
}

bool LinearBuffer::is_full() const
{
    return _size == _buffer.size();
}

void LinearBuffer::clear()
{
    _head = 0;
    _tail = 0;
    _size = 0;
}

void LinearBuffer::reset_if_empty()
{
    if (_size == 0)
    {
        _head = 0;
        _tail = 0;
    }
}
