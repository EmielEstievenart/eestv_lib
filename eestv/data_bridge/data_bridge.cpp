#include "eestv/data_bridge/data_bridge.hpp"
#include "eestv/logging/eestv_logging.hpp"

#include <chrono>
#include <memory>

namespace eestv
{

DataBridge::DataBridge(const DataBridgeConfig& config, boost::asio::io_context& io_context, int port)
    : _config(config)
    , _discoverable(_config.discovery_target, [this](const auto&) { return on_disvovered(); })
    , _io_context(io_context)
    , _default_port(port)
{
    // Default log level is Info (shows Error and Info)
    // EESTV_SET_LOG_LEVEL(Info);

    set_up_discovery();
}

DataBridge::~DataBridge()
{
}

void DataBridge::set_up_discovery()
{

    // Client needs: io_context, service_name, retry_timeout, port, response_handler
    auto response_handler = [this](const std::string& response, const boost::asio::ip::udp::endpoint& endpoint)
    { return on_peer_discovered(response, endpoint); };

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

std::string DataBridge::on_disvovered()
{
    ++_discovered_count;
    return "TEST";
}

bool DataBridge::on_peer_discovered(const std::string& response, const boost::asio::ip::udp::endpoint& endpoint)
{
    ++_discovery_count;
    EESTV_LOG_DEBUG("Discovery response received: " << response << " from " << endpoint.address().to_string() << ":" << endpoint.port());
    return true;
}

void DataBridge::set_up_tcp_server()
{
    _tcp_server = std::make_unique<TcpServer<>>(_io_context, 0);
};
} // namespace eestv