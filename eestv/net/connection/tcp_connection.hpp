#pragma once

#include "eestv/data/linear_buffer.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "tcp_connection_states.hpp"
#include <boost/asio.hpp>

#include <chrono>
#include <functional>
#include <memory>

namespace eestv
{

template <typename ReceiveBuffer = LinearBuffer, typename SendBuffer = LinearBuffer>
class TcpConnection : public std::enable_shared_from_this<TcpConnection<ReceiveBuffer, SendBuffer>>
{
public:
    using ConnectionLostCallback = std::function<void()>;
    using KeepAliveCallback      = std::function<std::pair<bool, std::vector<char>>()>;

    static constexpr std::size_t receive_buffer_size = 4096;
    static constexpr std::chrono::seconds default_keepalive_interval {5};

    virtual ~TcpConnection();

    TcpConnection(const TcpConnection&)            = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&&)                 = delete;
    TcpConnection& operator=(TcpConnection&&)      = delete;

    TcpConnectionState get_state() const { return _state; }
    bool is_connected() const { return _state == TcpConnectionState::connected || _state == TcpConnectionState::monitoring; }

    void set_connection_lost_callback(ConnectionLostCallback callback) { _connection_lost_callback = std::move(callback); }
    void set_keep_alive_callback(KeepAliveCallback callback) { _keep_alive_callback = std::move(callback); }

    // Access to buffers for user code
    ReceiveBuffer& receive_buffer() { return _receive_buffer; }
    const ReceiveBuffer& receive_buffer() const { return _receive_buffer; }
    SendBuffer& send_buffer() { return _send_buffer; }
    const SendBuffer& send_buffer() const { return _send_buffer; }

    // Trigger async send of data in send buffer
    void send();

    void start_monitoring();
    void disconnect();

protected:
    TcpConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context, std::size_t receive_buffer_size,
                  std::size_t send_buffer_size, std::chrono::seconds keepalive_interval = default_keepalive_interval);

    virtual void on_connection_lost() = 0;

    void set_state(TcpConnectionState new_state);
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
    void on_send(const boost::system::error_code& error, std::size_t bytes_transferred);

    TcpConnectionState _state;
    boost::asio::steady_timer _keepalive_timer;
    std::chrono::steady_clock::time_point _last_activity;
    ReceiveBuffer _receive_buffer;
    SendBuffer _send_buffer;
    bool _send_in_progress;
    ConnectionLostCallback _connection_lost_callback;
    KeepAliveCallback _keep_alive_callback;
};

// Template implementation

template <typename ReceiveBuffer, typename SendBuffer>
TcpConnection<ReceiveBuffer, SendBuffer>::TcpConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context,
                                                        std::size_t receive_buffer_size, std::size_t send_buffer_size,
                                                        std::chrono::seconds keepalive_interval)
    : _io_context {io_context}
    , _socket(std::move(socket))
    , _keep_alive_interval(keepalive_interval)
    , _state(TcpConnectionState::connected)
    , _keepalive_timer(_io_context)
    , _last_activity(std::chrono::steady_clock::now())
    , _receive_buffer(receive_buffer_size)
    , _send_buffer(send_buffer_size)
    , _send_in_progress(false)
{
    try
    {
        _remote_endpoint = _socket.remote_endpoint();
        // EESTV_LOG_DEBUG("TcpConnection established with " << _remote_endpoint.address().to_string() << ":" << _remote_endpoint.port());
    }
    catch (const boost::system::system_error& e)
    {
        // EESTV_LOG_ERROR("Could not get remote endpoint: " << e.what());
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
TcpConnection<ReceiveBuffer, SendBuffer>::~TcpConnection()
{
    disconnect();
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::disconnect()
{
    if (_state == TcpConnectionState::dead)
    {
        return;
    }

    set_state(TcpConnectionState::dead);

    _keepalive_timer.cancel();
    boost::system::error_code ec;
    if (_socket.is_open())
    {
        _socket.close(ec);
        // if (ec) { EESTV_LOG_DEBUG("Error closing socket: " << ec.message()); }
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::start_monitoring()
{
    if (_state == TcpConnectionState::dead)
    {
        return;
    }

    set_state(TcpConnectionState::monitoring);
    start_keepalive();
    start_receive();
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::set_state(TcpConnectionState new_state)
{
    if (_state != new_state)
    {
        // EESTV_LOG_DEBUG("State transition: " << to_string(_state) << " -> " << to_string(new_state));
        _state = new_state;
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::send()
{
    if (_state == TcpConnectionState::dead)
    {
        return;
    }

    // Don't start a new send if one is already in progress
    if (_send_in_progress)
    {
        return;
    }

    // Check if there's data to send
    std::size_t bytes_to_send;
    const std::uint8_t* send_ptr = _send_buffer.get_read_head(bytes_to_send);

    if (bytes_to_send == 0 || send_ptr == nullptr)
    {
        return;
    }

    _send_in_progress = true;

    boost::asio::async_write(_socket, boost::asio::buffer(send_ptr, bytes_to_send),
                             [self = this->shared_from_this()](const boost::system::error_code& ec, std::size_t bytes_transferred)
                             { self->on_send(ec, bytes_transferred); });
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::on_send(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    _send_in_progress = false;

    if (ec)
    {
        // EESTV_LOG_INFO("Send error: " << ec.message());
        set_state(TcpConnectionState::lost);
        on_connection_lost();
        if (_connection_lost_callback)
        {
            _connection_lost_callback();
        }
        return;
    }

    // Consume the sent data from the buffer
    _send_buffer.consume(bytes_transferred);

    _last_activity = std::chrono::steady_clock::now();

    // If there's more data to send, trigger another send
    std::size_t remaining_size;
    if (_send_buffer.get_read_head(remaining_size) != nullptr && remaining_size > 0)
    {
        send();
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::start_keepalive()
{
    if (_state == TcpConnectionState::dead)
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

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::on_keepalive_timer()
{
    auto now                 = std::chrono::steady_clock::now();
    auto time_since_activity = std::chrono::duration_cast<std::chrono::seconds>(now - _last_activity);

    if (time_since_activity >= _keep_alive_interval)
    {
        if (_state == TcpConnectionState::monitoring)
        {
            if (_socket.is_open())
            {
                send_keep_alive();
            }
            else
            {
                // EESTV_LOG_INFO("Socket closed, connection lost");
                set_state(TcpConnectionState::lost);
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

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::send_keep_alive()
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
                self->set_state(TcpConnectionState::lost);
                self->on_connection_lost();
                if (self->_connection_lost_callback)
                {
                    self->_connection_lost_callback();
                }
            }
        });
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::start_receive()
{
    if (_state == TcpConnectionState::dead)
    {
        return;
    }

    // Get write head for receiving data
    std::size_t writable_size = 0;
    std::uint8_t* write_head  = _receive_buffer.get_write_head(writable_size);

    if (write_head == nullptr || writable_size == 0)
    {
        // No space available, cannot receive - schedule retry after a short delay
        // This prevents spinning but allows the buffer to be cleared by user code
        return;
    }

    _socket.async_read_some(boost::asio::buffer(write_head, writable_size),
                            [self = this->shared_from_this()](const boost::system::error_code& ec, std::size_t bytes_transferred)
                            { self->on_receive(ec, bytes_transferred); });
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::on_receive(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    if (ec)
    {
        // EESTV_LOG_INFO("Receive error: " << ec.message());
        set_state(TcpConnectionState::lost);
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
