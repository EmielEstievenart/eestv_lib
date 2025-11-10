#pragma once

#include "boost/asio.hpp"
#include "eestv/flags/flags.hpp"
#include "eestv/net/discovery/discovery_states.hpp"

#include <mutex>
#include <string>
#include <chrono>
#include <array>
#include <functional>

namespace eestv
{

class UdpDiscoveryClient
{
public:
    UdpDiscoveryClient(boost::asio::io_context& io_context, const std::string& service_name, std::chrono::milliseconds retry_timeout,
                       int port,
                       std::function<bool(const std::string& response, const boost::asio::ip::udp::endpoint& remote_endpoint)> on_response);

    ~UdpDiscoveryClient();

    UdpDiscoveryClient(const UdpDiscoveryClient&)            = delete;
    UdpDiscoveryClient& operator=(const UdpDiscoveryClient&) = delete;
    UdpDiscoveryClient(UdpDiscoveryClient&&)                 = delete;
    UdpDiscoveryClient& operator=(UdpDiscoveryClient&&)      = delete;

    bool async_start();
    bool async_stop(std::function<void()> on_stopped);
    void stop();
    /*Must be called after a succesfull stop if you want to restart this object again*/
    void reset();

private:
    std::mutex _mutex;
    void send_discovery_request();
    void handle_send_complete(const boost::system::error_code& error_code);
    void handle_response(const boost::system::error_code& error, std::size_t bytes_transferred);
    void handle_timeout(const boost::system::error_code& error);

    void resolve_on_stopped();

    boost::asio::io_context& _io_context;
    std::string _service_name;
    boost::asio::ip::udp::socket _socket;
    boost::asio::ip::udp::endpoint _remote_endpoint;
    /// Size of the receive buffer in bytes.
    static constexpr std::size_t recv_buffer_size = 1024;
    std::array<char, recv_buffer_size> _recv_buffer;
    std::chrono::milliseconds _retry_timeout;
    boost::asio::steady_timer _timer;
    int _destination_port;
    std::function<bool(const std::string&, const boost::asio::ip::udp::endpoint& remote_endpoint)> _on_response;
    std::function<void()> _on_stopped;

    Flags<UdpDiscoveryState> _flags;
};
} // namespace eestv
