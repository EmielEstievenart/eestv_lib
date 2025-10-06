#pragma once

#include <memory>
#include <string>

#include <boost/asio/io_context.hpp>
#include <thread>

#include "eestv/net/discovery/udp_discovery_client.hpp"
#include "eestv/net/discovery/udp_discovery_server.hpp"

namespace eestv
{

enum class ClientServerMode
{
    client,
    server
};

enum class EndpointMode
{
    endpoint,
    bridge
};

class DataBridge
{
public:
    DataBridge(int argc, char* argv[]);
    ~DataBridge();

    ClientServerMode client_server_mode() const noexcept;
    EndpointMode endpoint_mode() const noexcept;
    const std::string& discovery_target() const noexcept;

private:
    void parse_command_line_parameters(int argc, char* argv[]);
    void set_up_discovery(ClientServerMode client_server_mode, const std::string& discovery_target);

    ClientServerMode _client_server_mode;
    EndpointMode _endpoint_mode;
    std::string _discovery_target;

    boost::asio::io_context _io_context;
    std::unique_ptr<UdpDiscoveryServer> _discovery_server;
    std::unique_ptr<UdpDiscoveryClient> _discovery_client;
    std::thread _io_thread;
};
} // namespace eestv
