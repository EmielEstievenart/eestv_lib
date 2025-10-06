#pragma once

#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include <boost/asio.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <array>

namespace eestv
{

/**
 * @brief Adapter for std::array to work with Connection's buffer interface
 * 
 * Provides data(), size(), and commit() methods for compatibility.
 */
template <std::size_t N>
class ArrayBufferAdapter
{
public:
    ArrayBufferAdapter() : _written(0) { }

    char* data() { return _buffer.data(); }
    std::size_t size() const { return N - _written; }

    bool commit(std::size_t bytes_written)
    {
        if (bytes_written > size())
        {
            return false;
        }
        _written += bytes_written;
        return true;
    }

    // Access to underlying buffer for reading
    const char* read_data() const { return _buffer.data(); }
    std::size_t read_size() const { return _written; }

    // Consume data
    void consume(std::size_t bytes)
    {
        if (bytes >= _written)
        {
            _written = 0;
        }
        else
        {
            std::memmove(_buffer.data(), _buffer.data() + bytes, _written - bytes);
            _written -= bytes;
        }
    }

private:
    std::array<char, N> _buffer;
    std::size_t _written;
};

template <typename ReceiveBuffer = ArrayBufferAdapter<4096>>
class Connection : public std::enable_shared_from_this<Connection<ReceiveBuffer>>
{
public:
    enum class State
    {
        connected,
        monitoring,
        lost,
        dead
    };

    inline const char* to_string(State state) noexcept
    {
        switch (state)
        {
        case State::connected:
            return "connected";
        case State::monitoring:
            return "monitoring";
        case State::lost:
            return "lost";
        case State::dead:
            return "dead";
        }
        return "unknown";
    }

    using ConnectionLostCallback = std::function<void()>;
    using KeepAliveCallback      = std::function<std::pair<bool, std::vector<char>>()>;

    static constexpr std::size_t receive_buffer_size = 4096;
    static constexpr std::chrono::seconds default_keepalive_interval {5};

    virtual ~Connection();

    Connection(const Connection&)            = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&)                 = delete;
    Connection& operator=(Connection&&)      = delete;

    State get_state() const { return _state; }
    bool is_connected() const { return _state == State::connected || _state == State::monitoring; }

    void set_connection_lost_callback(ConnectionLostCallback callback) { _connection_lost_callback = std::move(callback); }
    void set_keep_alive_callback(KeepAliveCallback callback) { _keep_alive_callback = std::move(callback); }

    void start_monitoring();
    void disconnect();

protected:
    Connection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context,
               std::chrono::seconds keepalive_interval = default_keepalive_interval);

    virtual void on_connection_lost() = 0;

    void set_state(State new_state);
    void start_keepalive();
    void start_receive();

    boost::asio::io_context& _io_context;
    boost::asio::ip::tcp::socket _socket;
    boost::asio::ip::tcp::endpoint _remote_endpoint;
    std::chrono::seconds _keep_alive_interval;

private:
    void on_keepalive_timer();
    void send_keep_alive();
    void on_receive(const boost::system::error_code& error, std::size_t bytes_transferred);

    State _state;
    boost::asio::steady_timer _keepalive_timer;
    std::chrono::steady_clock::time_point _last_activity;
    ReceiveBuffer _receive_buffer;
    ConnectionLostCallback _connection_lost_callback;
    KeepAliveCallback _keep_alive_callback;
};

// Template implementation

template <typename ReceiveBuffer>
Connection<ReceiveBuffer>::Connection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context,
                                      std::chrono::seconds keepalive_interval)
    : _io_context {io_context}
    , _socket(std::move(socket))
    , _keep_alive_interval(keepalive_interval)
    , _state(State::connected)
    , _keepalive_timer(_io_context)
    , _last_activity(std::chrono::steady_clock::now())
    , _receive_buffer {}
{
    try
    {
        _remote_endpoint = _socket.remote_endpoint();
        // EESTV_LOG_DEBUG("Connection established with " << _remote_endpoint.address().to_string() << ":" << _remote_endpoint.port());
    }
    catch (const boost::system::system_error& e)
    {
        // EESTV_LOG_ERROR("Could not get remote endpoint: " << e.what());
    }
}

template <typename ReceiveBuffer>
Connection<ReceiveBuffer>::~Connection()
{
    disconnect();
}

template <typename ReceiveBuffer>
void Connection<ReceiveBuffer>::disconnect()
{
    if (_state == State::dead)
    {
        return;
    }

    set_state(State::dead);

    _keepalive_timer.cancel();
    boost::system::error_code ec;
    if (_socket.is_open())
    {
        _socket.close(ec);
        // if (ec) { EESTV_LOG_DEBUG("Error closing socket: " << ec.message()); }
    }
}

template <typename ReceiveBuffer>
void Connection<ReceiveBuffer>::start_monitoring()
{
    if (_state == State::dead)
    {
        return;
    }

    set_state(State::monitoring);
    start_keepalive();
    start_receive();
}

template <typename ReceiveBuffer>
void Connection<ReceiveBuffer>::set_state(State new_state)
{
    if (_state != new_state)
    {
        // EESTV_LOG_DEBUG("State transition: " << to_string(_state) << " -> " << to_string(new_state));
        _state = new_state;
    }
}

template <typename ReceiveBuffer>
void Connection<ReceiveBuffer>::start_keepalive()
{
    if (_state == State::dead)
    {
        return;
    }

    auto now        = std::chrono::steady_clock::now();
    auto since_last = now - _last_activity;

    std::chrono::milliseconds wait_for;
    if (since_last >= _keep_alive_interval)
    {
        wait_for = std::chrono::milliseconds(0);
    }
    else
    {
        wait_for = std::chrono::duration_cast<std::chrono::milliseconds>(_keep_alive_interval - since_last);
    }

    _keepalive_timer.expires_after(wait_for);
    _keepalive_timer.async_wait(
        [self = this->shared_from_this()](const boost::system::error_code& ec)
        {
            if (!ec)
            {
                self->on_keepalive_timer();
            }
        });
}

template <typename ReceiveBuffer>
void Connection<ReceiveBuffer>::on_keepalive_timer()
{
    auto now                 = std::chrono::steady_clock::now();
    auto time_since_activity = std::chrono::duration_cast<std::chrono::seconds>(now - _last_activity);

    if (time_since_activity >= _keep_alive_interval)
    {
        if (_state == State::monitoring)
        {
            if (_socket.is_open())
            {
                send_keep_alive();
            }
            else
            {
                // EESTV_LOG_INFO("Socket closed, connection lost");
                set_state(State::lost);
                on_connection_lost();
                if (_connection_lost_callback)
                {
                    _connection_lost_callback();
                }
                return;
            }
        }
    }

    start_keepalive();
}

template <typename ReceiveBuffer>
void Connection<ReceiveBuffer>::send_keep_alive()
{
    // If no callback is set, skip keep-alive sending
    if (!_keep_alive_callback)
    {
        return;
    }

    // Call the user-provided callback to get keep-alive data
    auto [should_send, keep_alive_data] = _keep_alive_callback();

    if (!should_send || keep_alive_data.empty())
    {
        return;
    }

    // Create a shared buffer to keep the data alive during async operation
    auto data_buffer = std::make_shared<std::vector<char>>(std::move(keep_alive_data));

    boost::asio::async_write(
        _socket, boost::asio::buffer(*data_buffer),
        [self = this->shared_from_this(), data_buffer](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
        {
            if (ec)
            {
                // EESTV_LOG_INFO("Keep-alive failed: " << ec.message());
                self->set_state(State::lost);
                self->on_connection_lost();
                if (self->_connection_lost_callback)
                {
                    self->_connection_lost_callback();
                }
            }
        });
}

template <typename ReceiveBuffer>
void Connection<ReceiveBuffer>::start_receive()
{
    if (_state == State::dead)
    {
        return;
    }

    // Use buffer interface: data() returns pointer, size() returns available space
    _socket.async_read_some(boost::asio::buffer(_receive_buffer.data(), _receive_buffer.size()),
                            [self = this->shared_from_this()](const boost::system::error_code& ec, std::size_t bytes_transferred)
                            { self->on_receive(ec, bytes_transferred); });
}

template <typename ReceiveBuffer>
void Connection<ReceiveBuffer>::on_receive(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    if (ec)
    {
        // EESTV_LOG_INFO("Receive error: " << ec.message());
        set_state(State::lost);
        on_connection_lost();
        if (_connection_lost_callback)
        {
            _connection_lost_callback();
        }
        return;
    }

    // Commit the written bytes to the buffer
    _receive_buffer.commit(bytes_transferred);

    _last_activity = std::chrono::steady_clock::now();

    // TODO: Process received data
    // EESTV_LOG_DEBUG("Received " << bytes_transferred << " bytes");

    start_receive();
}

} // namespace eestv