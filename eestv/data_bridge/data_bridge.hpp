#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "data_bridge_config.hpp"
#include "eestv/net/discovery/discoverable.hpp"
#include "eestv/net/discovery/discovery_string.hpp"
#include "eestv/net/discovery/udp_discovery_client.hpp"
#include "eestv/net/discovery/udp_discovery_server.hpp"
#include "eestv/net/connection/tcp_connection.hpp"
#include "eestv/net/connection/tcp_server.hpp"

namespace eestv
{

class DataBridge
{
public:
    static constexpr int default_port = 12345;

    DataBridge(const DataBridgeConfig& config, boost::asio::io_context& io_context, int port = default_port);
    ~DataBridge();

    EndpointMode endpoint_mode() const noexcept { return _config.endpoint_mode; }
    const std::string& discovery_target() const noexcept { return _config.discovery_target; }

    // Analytics
    std::size_t discovery_count() const noexcept { return _discovery_count; }
    std::size_t discovered_count() const noexcept { return _discovered_count; }

private:
    void set_up_discovery();
    void set_up_tcp_server();

    bool on_response_from_peer(const std::string& response, const boost::asio::ip::udp::endpoint& endpoint);
    bool on_peer_discovered(DiscoveryInfo& discovery_info);

    DataBridgeConfig _config;

    Discoverable _discoverable;

    boost::asio::io_context& _io_context;
    std::unique_ptr<UdpDiscoveryServer> _discovery_server;
    std::unique_ptr<UdpDiscoveryClient> _discovery_client;
    std::unique_ptr<TcpServer<>> _tcp_server;

    std::vector<std::unique_ptr<TcpConnection<>>> _connections;

    int _default_port = 12345;

    // Analytics
    std::size_t _discovery_count  = 0; // Number of times we discovered peers
    std::size_t _discovered_count = 0; // Number of times we were discovered

    std::string on_disvovered(const boost::asio::ip::udp::endpoint& remote_endpoint);
};
} // namespace eestv
