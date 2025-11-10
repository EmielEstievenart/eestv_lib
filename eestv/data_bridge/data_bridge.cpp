#include "eestv/data_bridge/data_bridge.hpp"
#include "eestv/logging/eestv_logging.hpp"
#include "eestv/net/network_utils.hpp"
#include "eestv/net/discovery/discovery_string.hpp"

#include <chrono>
#include <memory>

namespace eestv
{

DataBridge::DataBridge(const DataBridgeConfig& config, boost::asio::io_context& io_context, int port)
    : _config(config)
    , _discoverable(_config.discovery_target,
                    [this](const boost::asio::ip::udp::endpoint& remote_endpoint) { return on_disvovered(remote_endpoint); })
    , _io_context(io_context)
    , _default_port(port)
{
    // Default log level is Info (shows Error and Info)
    // EESTV_SET_LOG_LEVEL(Info);

    set_up_tcp_server();
    set_up_discovery();
}

DataBridge::~DataBridge()
{
}

void DataBridge::set_up_discovery()
{

    // Client needs: io_context, service_name, retry_timeout, port, response_handler
    auto response_handler = [this](const std::string& response, const boost::asio::ip::udp::endpoint& endpoint)
    { return on_response_from_peer(response, endpoint); };

    _discovery_client = std::make_unique<UdpDiscoveryClient>(_io_context, _config.discovery_target, std::chrono::milliseconds(1000),
                                                             _default_port, response_handler);

    _discovery_client->async_start();

    EESTV_LOG_INFO("Discovery client initialized for target: " << _config.discovery_target);

    // Server needs: io_context, port
    _discovery_server = std::make_unique<UdpDiscoveryServer>(_io_context, _default_port);
    _discovery_server->add_discoverable(_discoverable);
    _discovery_server->async_start();
    EESTV_LOG_INFO("Discovery server initialized for target: " << _config.discovery_target);
}

std::string DataBridge::on_disvovered(const boost::asio::ip::udp::endpoint& remote_endpoint)
{
    ++_discovered_count;

    auto local_address = get_local_address_for_remote(_io_context, remote_endpoint);

    unsigned short tcp_port = _tcp_server->port();

    std::string response;
    if (local_address)
    {
        response = DiscoveryString::construct(_config.discovery_target, *local_address, tcp_port);
    }
    else
    {
        // Log warning if we can't determine local address - discovery should fail
        EESTV_LOG_WARNING("Could not determine local address for discovery response from " << remote_endpoint.address().to_string());
    }

    if (!response.empty())
    {
        EESTV_LOG_DEBUG("Discovery response: " << response << " for remote " << remote_endpoint.address().to_string());
    }
    return response;
}

bool DataBridge::on_response_from_peer(const std::string& response, const boost::asio::ip::udp::endpoint& endpoint)
{
    auto discovery_info = DiscoveryString::parse(response);
    if (discovery_info.has_value() && discovery_info->service_name == _config.discovery_target)
    {
        on_peer_discovered(discovery_info.value());
        EESTV_LOG_DEBUG("Discovered service '" << discovery_info->service_name << "' at " << discovery_info->ip_address << ":"
                                               << discovery_info->port << " (UDP endpoint: " << endpoint.address().to_string() << ":"
                                               << endpoint.port() << ")");
    }
    else
    {
        EESTV_LOG_WARNING("Failed to parse discovery response: " << response);
        return false;
    }
    return true;
}

bool DataBridge::on_peer_discovered(DiscoveryInfo& discovery_info)
{
    ++_discovery_count;

    // TODO: Use discovery_info to establish TCP connection to the discovered service
    // The endpoint field contains the full TCP endpoint that can be used for connection

    return true;
}

void DataBridge::set_up_tcp_server()
{
    _tcp_server = std::make_unique<TcpServer<>>(_io_context, 0);
    _tcp_server->async_start();
    EESTV_LOG_INFO("TCP server started on port " << _tcp_server->port());
};

} // namespace eestv