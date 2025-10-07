#pragma once

#include "eestv/net/connection/tcp_connection.hpp"

namespace eestv
{

template <typename ReceiveBuffer = LinearBuffer, typename SendBuffer = LinearBuffer>
class TcpClientConnection : public TcpConnection<ReceiveBuffer, SendBuffer>
{
public:
    static constexpr std::chrono::milliseconds default_reconnect_delay {1000};
    static constexpr std::chrono::milliseconds max_reconnect_delay {30000};
    static constexpr int max_reconnect_attempts      = -1; // -1 means infinite
    static constexpr std::size_t default_buffer_size = 4096;

    TcpClientConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context,
                        std::size_t receive_buffer_size = default_buffer_size, std::size_t send_buffer_size = default_buffer_size,
                        std::chrono::seconds keepalive_interval = TcpConnection<ReceiveBuffer, SendBuffer>::default_keepalive_interval);

    TcpClientConnection(boost::asio::ip::tcp::endpoint remote_endpoint, boost::asio::io_context& io_context,
                        std::size_t receive_buffer_size = default_buffer_size, std::size_t send_buffer_size = default_buffer_size,
                        std::chrono::seconds keepalive_interval = TcpConnection<ReceiveBuffer, SendBuffer>::default_keepalive_interval);

    ~TcpClientConnection() override = default;

    void set_auto_reconnect(bool enabled) { _auto_reconnect = enabled; }
    bool get_auto_reconnect() const { return _auto_reconnect; }

    void set_max_reconnect_attempts(int max_attempts) { _max_reconnect_attempts = max_attempts; }
    int get_max_reconnect_attempts() const { return _max_reconnect_attempts; }

    void connect();
    int get_reconnect_attempts() const { return _reconnect_attempts; }

protected:
    void on_connection_lost() override;

private:
    void attempt_reconnect();
    void handle_connect_result(const boost::system::error_code& error_code);

    bool _auto_reconnect {true};
    int _max_reconnect_attempts {max_reconnect_attempts};
    int _reconnect_attempts {0};
    std::chrono::milliseconds _current_reconnect_delay {default_reconnect_delay};
    std::unique_ptr<boost::asio::steady_timer> _reconnect_timer;
};

// Template implementation

template <typename ReceiveBuffer, typename SendBuffer>
TcpClientConnection<ReceiveBuffer, SendBuffer>::TcpClientConnection(boost::asio::ip::tcp::socket&& socket,
                                                                    boost::asio::io_context& io_context, std::size_t receive_buffer_size,
                                                                    std::size_t send_buffer_size, std::chrono::seconds keepalive_interval)
    : TcpConnection<ReceiveBuffer, SendBuffer>(std::move(socket), io_context, receive_buffer_size, send_buffer_size, keepalive_interval)
{
}

template <typename ReceiveBuffer, typename SendBuffer>
TcpClientConnection<ReceiveBuffer, SendBuffer>::TcpClientConnection(boost::asio::ip::tcp::endpoint remote_endpoint,
                                                                    boost::asio::io_context& io_context, std::size_t receive_buffer_size,
                                                                    std::size_t send_buffer_size, std::chrono::seconds keepalive_interval)
    : TcpConnection<ReceiveBuffer, SendBuffer>(boost::asio::ip::tcp::socket(io_context), io_context, receive_buffer_size, send_buffer_size,
                                               keepalive_interval)
{
    this->_remote_endpoint = remote_endpoint;
    this->set_state(TcpConnectionState::dead);
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpClientConnection<ReceiveBuffer, SendBuffer>::connect()
{
    if (this->get_state() != TcpConnectionState::dead)
    {
        // EESTV_LOG_DEBUG("TcpConnection already active, ignoring connect request");
        return;
    }

    // EESTV_LOG_INFO("Attempting to connect to " << this->_remote_endpoint.address().to_string() << ":" << this->_remote_endpoint.port());

    this->_socket.async_connect(this->_remote_endpoint,
                                [self = std::static_pointer_cast<TcpClientConnection<ReceiveBuffer, SendBuffer>>(this->shared_from_this())](
                                    const boost::system::error_code& error_code) { self->handle_connect_result(error_code); });
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpClientConnection<ReceiveBuffer, SendBuffer>::handle_connect_result(const boost::system::error_code& error_code)
{
    if (!error_code)
    {
        // EESTV_LOG_INFO("Successfully connected");
        _reconnect_attempts      = 0;
        _current_reconnect_delay = default_reconnect_delay;
        this->set_state(TcpConnectionState::connected);
        this->start_monitoring();
    }
    else
    {
        // EESTV_LOG_ERROR("TcpConnection failed: " << error_code.message());
        _reconnect_attempts++;

        if (_auto_reconnect && (_max_reconnect_attempts < 0 || _reconnect_attempts < _max_reconnect_attempts))
        {
            attempt_reconnect();
        }
        else
        {
            // EESTV_LOG_ERROR("Max reconnection attempts reached or auto-reconnect disabled");
            this->set_state(TcpConnectionState::dead);
        }
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpClientConnection<ReceiveBuffer, SendBuffer>::on_connection_lost()
{
    // EESTV_LOG_INFO("Client connection lost");

    if (_auto_reconnect)
    {
        attempt_reconnect();
    }
    else
    {
        this->set_state(TcpConnectionState::dead);
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpClientConnection<ReceiveBuffer, SendBuffer>::attempt_reconnect()
{
    if (_max_reconnect_attempts >= 0 && _reconnect_attempts >= _max_reconnect_attempts)
    {
        // EESTV_LOG_ERROR("Max reconnection attempts reached");
        this->set_state(TcpConnectionState::dead);
        return;
    }

    // EESTV_LOG_INFO("Scheduling reconnection attempt");

    boost::system::error_code error_code;
    if (this->_socket.is_open())
    {
        this->_socket.close(error_code);
    }

    _reconnect_timer = std::make_unique<boost::asio::steady_timer>(this->_io_context, _current_reconnect_delay);
    _reconnect_timer->async_wait(
        [self = std::static_pointer_cast<TcpClientConnection<ReceiveBuffer, SendBuffer>>(this->shared_from_this())](
            const boost::system::error_code& error_code)
        {
            if (!error_code)
            {
                try
                {
                    self->_socket = boost::asio::ip::tcp::socket(self->_io_context);
                    self->connect();
                }
                catch (const std::exception& exception)
                {
                    // EESTV_LOG_ERROR("Exception during reconnect");
                    self->set_state(TcpConnectionState::dead);
                }
            }
            else if (error_code != boost::asio::error::operation_aborted)
            {
                // EESTV_LOG_ERROR("Reconnect timer error");
                self->set_state(TcpConnectionState::dead);
            }
        });

    _current_reconnect_delay = std::min(_current_reconnect_delay * 2, max_reconnect_delay);
}

} // namespace eestv
