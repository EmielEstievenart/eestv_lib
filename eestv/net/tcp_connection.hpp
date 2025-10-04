#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <array>
#include <cstdint>

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{

public:
    // Connection states
    enum class State : std::uint8_t
    {
        connected,
        disconnected,
        reconnecting
    };

    // Default alive check interval (1 second)
    static constexpr std::chrono::seconds default_alive_check_interval {1};

    // Receive buffer size
    static constexpr std::size_t RECEIVE_BUFFER_SIZE = 1024;

    // Constructor that takes an already established socket
    explicit TcpConnection(boost::asio::ip::tcp::socket&& socket, std::chrono::seconds alive_check_interval = default_alive_check_interval);

    // Destructor
    ~TcpConnection();

    // Delete copy constructor and assignment operator
    TcpConnection(const TcpConnection&)            = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    // Delete move constructor and assignment operator
    TcpConnection(TcpConnection&&)            = delete;
    TcpConnection& operator=(TcpConnection&&) = delete;

    // Manually disconnect the connection
    void disconnect();

    // Get current connection state
    State get_state() const { return _state; }

    // Check if connection is active
    bool is_connected() const { return _state == State::connected; }

private:
    // Start the keep-alive mechanism
    void start_keep_alive();

    // Handle keep-alive timer expiration
    void on_keep_alive_timer();

    // Send ping message
    void send_ping();

    // Handle incoming data
    void start_receive();
    void on_receive(const boost::system::error_code& error, std::size_t bytes_transferred);

    // Handle connection state changes
    void set_state(State new_state);

    // Attempt to reconnect
    void attempt_reconnect();

    boost::asio::ip::tcp::socket _socket;
    std::chrono::seconds _alive_check_interval;
    State _state;
    boost::asio::steady_timer _keep_alive_timer;
    std::chrono::steady_clock::time_point _last_activity;
    bool _user_disconnected;
    std::array<char, RECEIVE_BUFFER_SIZE> _receive_buffer;
    boost::asio::ip::tcp::endpoint _remote_endpoint;
};