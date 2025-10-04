
#include "eestv/net/tcp_connection.hpp"
#include <iostream>

TcpConnection::TcpConnection(boost::asio::ip::tcp::socket&& socket, std::chrono::seconds alive_check_interval)
    : _socket(std::move(socket))
    , _alive_check_interval(alive_check_interval)
    , _state(State::connected)
    , _keep_alive_timer(_socket.get_executor())
    , _last_activity(std::chrono::steady_clock::now())
    , _user_disconnected(false)
    , _receive_buffer {}
{
    // Store the remote endpoint for potential reconnection
    try {
        _remote_endpoint = _socket.remote_endpoint();
        std::cout << "Connected to: " << _remote_endpoint << '\n';
    } catch (const boost::system::system_error& e) {
        std::cout << "Warning: Could not get remote endpoint: " << e.what() << '\n';
    }

    // Start keep-alive mechanism and receiving data
    start_keep_alive();
    start_receive();
}

TcpConnection::~TcpConnection()
{
    disconnect();
}

void TcpConnection::disconnect()
{
    _user_disconnected = true;
    set_state(State::disconnected);

    _keep_alive_timer.cancel();
    boost::system::error_code error_code;
    if (_socket.is_open())
    {
        _socket.close(error_code);
        // Note: close() doesn't return a value, the error_code parameter captures any errors
        if (error_code)
        {
            std::cout << "Error closing socket: " << error_code.message() << '\n';
        }
    }
}

void TcpConnection::start_keep_alive()
{
    if (_state == State::disconnected)
    {
        return;
    }

    _keep_alive_timer.expires_after(_alive_check_interval);
    _keep_alive_timer.async_wait(
        [self = shared_from_this()](const boost::system::error_code& error)
        {
            if (!error)
            {
                self->on_keep_alive_timer();
            }
        });
}

void TcpConnection::on_keep_alive_timer()
{
    auto now                 = std::chrono::steady_clock::now();
    auto time_since_activity = std::chrono::duration_cast<std::chrono::seconds>(now - _last_activity);

    if (time_since_activity >= _alive_check_interval)
    {
        if (_state == State::connected)
        {
            // No activity detected, send ping or consider connection lost
            if (_socket.is_open())
            {
                send_ping();
            }
            else
            {
                // Socket is closed, transition to reconnecting if not user-initiated
                if (!_user_disconnected)
                {
                    set_state(State::reconnecting);
                    attempt_reconnect();
                    return;
                }
            }
        }
    }

    // Schedule next keep-alive check
    start_keep_alive();
}

void TcpConnection::send_ping()
{
    // Simple ping message - in a real implementation you might use a specific protocol
    static const std::string ping_msg = "PING\n";

    boost::asio::async_write(_socket, boost::asio::buffer(ping_msg),
                             [self = shared_from_this()](const boost::system::error_code& error, std::size_t /*bytes_transferred*/)
                             {
                                 if (error)
                                 {
                                     // Write failed, connection might be lost
                                     if (!self->_user_disconnected)
                                     {
                                         self->set_state(State::reconnecting);
                                         self->attempt_reconnect();
                                     }
                                 }
                             });
}

void TcpConnection::start_receive()
{
    if (_state == State::disconnected)
    {
        return;
    }

    _socket.async_read_some(boost::asio::buffer(_receive_buffer),
                            [self = shared_from_this()](const boost::system::error_code& error, std::size_t bytes_transferred)
                            { self->on_receive(error, bytes_transferred); });
}

void TcpConnection::on_receive(const boost::system::error_code& error, std::size_t bytes_transferred)
{
    if (error)
    {
        // Receive failed, connection might be lost
        if (!_user_disconnected)
        {
            set_state(State::reconnecting);
            attempt_reconnect();
        }
        return;
    }

    // Update last activity time
    _last_activity = std::chrono::steady_clock::now();

    // Process received data (for now just log it)
    std::string received_data(_receive_buffer.data(), bytes_transferred);
    std::cout << "Received: " << received_data << '\n';

    // Continue receiving
    start_receive();
}

void TcpConnection::set_state(State new_state)
{
    if (_state != new_state)
    {
        std::cout << "State transition: " << static_cast<int>(_state) << " -> " << static_cast<int>(new_state) << '\n';
        _state = new_state;
    }
}

void TcpConnection::attempt_reconnect()
{
    if (_user_disconnected || _state != State::reconnecting)
    {
        return;
    }

    std::cout << "Attempting to reconnect to: " << _remote_endpoint << '\n';

    // Create a new socket for reconnection
    auto new_socket = std::make_unique<boost::asio::ip::tcp::socket>(_socket.get_executor());
    
    // Attempt to connect to the stored remote endpoint
    new_socket->async_connect(_remote_endpoint,
        [self = shared_from_this(), socket_ptr = new_socket.release()](const boost::system::error_code& error)
        {
            std::unique_ptr<boost::asio::ip::tcp::socket> socket(socket_ptr);
            
            if (!error && !self->_user_disconnected)
            {
                // Reconnection successful
                std::cout << "Reconnection successful!\n";
                
                // Replace the old socket with the new connected one
                self->_socket = std::move(*socket);
                self->set_state(State::connected);
                self->_last_activity = std::chrono::steady_clock::now();
                
                // Restart keep-alive and receiving
                self->start_keep_alive();
                self->start_receive();
            }
            else
            {
                // Reconnection failed, wait and try again or give up
                std::cout << "Reconnection failed: " << (error ? error.message() : "User disconnected") << '\n';
                
                if (!self->_user_disconnected)
                {
                    // Reconnection timeout constant
                    static constexpr std::chrono::seconds RECONNECTION_RETRY_DELAY {5};
                    
                    // Wait before retrying or giving up
                    self->_keep_alive_timer.expires_after(RECONNECTION_RETRY_DELAY);
                    self->_keep_alive_timer.async_wait(
                        [self](const boost::system::error_code& timer_error)
                        {
                            if (!timer_error && !self->_user_disconnected && self->_state == State::reconnecting)
                            {
                                // Try reconnecting again
                                self->attempt_reconnect();
                            }
                            else
                            {
                                // Give up and transition to disconnected
                                self->set_state(State::disconnected);
                            }
                        });
                }
                else
                {
                    self->set_state(State::disconnected);
                }
            }
        });
};