#pragma once

#include "discoverable.hpp"
#include "boost/asio.hpp"
#include "boost/asio/io_context.hpp"
#include "eestv/flags/flags.hpp"
#include "eestv/net/discovery/discovery_states.hpp"
#include <unordered_map>
#include <array>
#include <mutex>
#include <functional>

namespace eestv
{

class UdpDiscoveryServer
{
public:
    UdpDiscoveryServer(boost::asio::io_context& io_context, int port);
    ~UdpDiscoveryServer();

    UdpDiscoveryServer(const UdpDiscoveryServer&)            = delete;
    UdpDiscoveryServer& operator=(const UdpDiscoveryServer&) = delete;
    UdpDiscoveryServer(UdpDiscoveryServer&&)                 = delete;
    UdpDiscoveryServer& operator=(UdpDiscoveryServer&&)      = delete;

    bool async_start();

    bool async_stop(std::function<void()> on_stopped);

    void stop();

    void reset();

    void add_discoverable(const Discoverable& discoverable);

private:
    std::mutex _mutex;

    void start_receive();
    void handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred);
    void handle_send(const boost::system::error_code& error, std::size_t bytes_transferred);
    void resolve_on_stopped();

    boost::asio::io_context& _io_context;
    boost::asio::ip::udp::socket _socket;
    boost::asio::ip::udp::endpoint _remote_endpoint;

    static constexpr std::size_t recv_buffer_size = 1024;
    std::array<char, recv_buffer_size> _recv_buffer;
    std::unordered_map<std::string, Discoverable> _discoverables;
    mutable std::mutex _discoverables_mutex;
    int _port;

    std::function<void()> _on_stopped;
    Flags<UdpDiscoveryState> _flags;
};
} // namespace eestv
