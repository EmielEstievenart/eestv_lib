#pragma once

#include "boost/asio.hpp"
#include <string>
#include <chrono>
#include <array>
#include <functional>

class UdpDiscoveryClient
{
public:
    UdpDiscoveryClient(boost::asio::io_context& io_context, const std::string& service_name, std::chrono::milliseconds retry_timeout,
                       int port, std::function<bool(const std::string&, const boost::asio::ip::udp::endpoint&)> response_handler);

    void start();
    void stop();

private:
    void send_discovery_request();
    void handle_response(const boost::system::error_code& error, std::size_t bytes_transferred);
    void handle_timeout(const boost::system::error_code& error);

    boost::asio::io_context& _io_context;
    std::string _service_name;
    boost::asio::ip::udp::socket _socket;
    boost::asio::ip::udp::endpoint _remote_endpoint;
    std::array<char, 1024> _recv_buffer;
    std::chrono::milliseconds _retry_timeout;
    boost::asio::steady_timer _timer;
    bool _running;
    int _port;
    std::function<bool(const std::string&, const boost::asio::ip::udp::endpoint&)> _response_handler;
};