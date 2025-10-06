#pragma once

#include <string>

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

    ClientServerMode client_server_mode() const noexcept;
    EndpointMode endpoint_mode() const noexcept;
    const std::string& discovery_target() const noexcept;

private:
    void parse_command_line_parameters(int argc, char* argv[]);

    ClientServerMode _client_server_mode;
    EndpointMode _endpoint_mode;
    std::string _discovery_target;
};
} // namespace eestv
