#include "eestv/data_bridge/bridge.hpp"
#include "eestv/logging/eestv_logging.hpp"
#include "eestv/net/network_utils.hpp"
#include "eestv/net/discovery/discovery_string.hpp"

#include <chrono>
#include <memory>
#include <optional>

namespace eestv::bridge
{

Bridge::Bridge(const Config& config, boost::asio::io_context& io_context, int port)
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

Bridge::~Bridge()
{
    _discovery_client->stop();
    _discovery_server->stop();
    _tcp_server->stop();

    for (auto& pair : _connecting_clients)
    {
        pair.second->stop();
    }
}

void Bridge::set_up_discovery()
{

    // Client needs: io_context, service_name, retry_timeout, port, response_handler
    auto response_handler = [this](const std::string& response, const boost::asio::ip::udp::endpoint& endpoint)
    { return on_discovery_response(response, endpoint); };

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

std::string Bridge::on_disvovered(const boost::asio::ip::udp::endpoint& remote_endpoint)
{
    ++_discovered_count;

    auto local_address      = get_local_address_for_remote(_io_context, remote_endpoint);
    unsigned short tcp_port = 0;
    if (_tcp_server != nullptr)
    {
        tcp_port = _tcp_server->port();
    }
    else
    {
        EESTV_LOG_ERROR("Wow, the tcp_server isn't initialized. ");
    }

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

bool Bridge::on_discovery_response(const std::string& response, const boost::asio::ip::udp::endpoint& endpoint)
{
    std::optional<DiscoveryInfo> discovery_info = DiscoveryString::parse(response);
    if (discovery_info.has_value() && discovery_info->service_name == _config.discovery_target)
    {
        on_peer_discovered(*discovery_info);
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

/**
 * @brief Handles the discovery of a new peer via UDP broadcast.
 *
 * This function is called when the UDP discovery mechanism detects a new peer.
 * It attempts to establish a TCP connection to the discovered peer if not already
 * connected or in the process of connecting. The connection is keyed by the
 * discovery timestamp to ensure uniqueness.
 *
 * @param discovery_info Information about the discovered peer, including IP, port, and timestamp.
 * @return true if the discovery was handled successfully.
 */
bool Bridge::on_peer_discovered(DiscoveryInfo& discovery_info)
{
    ++_discovery_count;

    std::string key = std::to_string(discovery_info.timestamp_ms);

    // Check if already connected or connecting
    if (_pending_outgoing_connections.find(key) != _pending_outgoing_connections.end() ||
        _connecting_clients.find(key) != _connecting_clients.end())
    {
        EESTV_LOG_DEBUG("Already connected or connecting to " << key);
        return true;
    }

    // Create TcpClient and attempt connection
    auto client     = std::make_unique<TcpClient>(_io_context);
    auto client_ptr = client.get();

    bool started =
        client->async_connect(discovery_info.ip_address, discovery_info.port,
                              [this, key, client_ptr](boost::asio::ip::tcp::socket&& socket, const boost::system::error_code& error)
                              {
                                  if (!error)
                                  {
                                      EESTV_LOG_DEBUG("Successfully connected to peer " << key);
                                      auto connection = std::make_unique<TcpConnection<>>(std::move(socket), _io_context, 4096, 4096);
                                      _pending_outgoing_connections[key] = std::move(connection);
                                  }
                                  else
                                  {
                                      EESTV_LOG_ERROR("Failed to connect to peer " << key << ": " << error.message());
                                  }
                                  // Remove from connecting clients
                                  //   _connecting_clients.erase(key);
                              });

    if (started)
    {
        EESTV_LOG_DEBUG("Started connection attempt to " << key);
        _connecting_clients[key] = std::move(client);
    }
    else
    {
        EESTV_LOG_ERROR("Failed to start connection to " << key);
    }

    return true;
}

void Bridge::on_accept(std::unique_ptr<TcpConnection<>> connection)
{
    auto remote_endpoint = connection->remote_endpoint();
    std::string key      = remote_endpoint.address().to_string() + ":" + std::to_string(remote_endpoint.port());

    _pending_incoming_connections[key] = std::move(connection);

    EESTV_LOG_DEBUG("Accepted connection from " << key << " and added to pending unique connections");
}

void Bridge::set_up_tcp_server()
{
    _tcp_server = std::make_unique<TcpServer<>>(_io_context, 0);
    _tcp_server->set_connection_callback([this](std::unique_ptr<TcpConnection<>> connection) { on_accept(std::move(connection)); });
    _tcp_server->async_start();
    EESTV_LOG_INFO("TCP server started on port " << _tcp_server->port());
};

} // namespace eestv::bridge