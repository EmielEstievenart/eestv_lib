#include "discovery_string.hpp"

#include "eestv/logging/eestv_logging.hpp"

#include <boost/asio/ip/tcp.hpp>

namespace eestv
{

std::string DiscoveryString::construct(const std::string& service_name, const std::string& ip_address, unsigned short port)
{
    return service_name + ":" + ip_address + ":" + std::to_string(port);
}

std::optional<DiscoveryInfo> DiscoveryString::parse(const std::string& response)
{
    // Expected format: "SERVICE_NAME:IP:PORT" (e.g., "my_service:192.168.1.100:54321")

    // Find the first colon separator (between service name and IP)
    size_t first_colon_pos = response.find(':');
    if (first_colon_pos == std::string::npos)
    {
        EESTV_LOG_ERROR("Invalid discovery response format (missing first colon): " << response);
        return std::nullopt;
    }

    // Find the second colon separator (between IP and port)
    size_t second_colon_pos = response.find(':', first_colon_pos + 1);
    if (second_colon_pos == std::string::npos)
    {
        EESTV_LOG_ERROR("Invalid discovery response format (missing second colon): " << response);
        return std::nullopt;
    }

    // Split into service name, IP, and port parts
    std::string service_name = response.substr(0, first_colon_pos);
    std::string ip_str       = response.substr(first_colon_pos + 1, second_colon_pos - first_colon_pos - 1);
    std::string port_str     = response.substr(second_colon_pos + 1);

    // Validate service name
    if (service_name.empty())
    {
        EESTV_LOG_ERROR("Invalid discovery response format (empty service name): " << response);
        return std::nullopt;
    }

    // Validate IP address format (basic check)
    if (ip_str.empty())
    {
        EESTV_LOG_ERROR("Invalid discovery response format (empty IP): " << response);
        return std::nullopt;
    }

    // Parse port number
    unsigned short port = 0;
    try
    {
        port = static_cast<unsigned short>(std::stoi(port_str));
    }
    catch (const std::exception& e)
    {
        EESTV_LOG_ERROR("Invalid discovery response format (invalid port '" << port_str << "'): " << response);
        return std::nullopt;
    }

    // Create the endpoint
    boost::system::error_code ec;
    auto address = boost::asio::ip::make_address(ip_str, ec);
    if (ec)
    {
        EESTV_LOG_ERROR("Invalid discovery response format (invalid IP '" << ip_str << "'): " << response);
        return std::nullopt;
    }

    boost::asio::ip::tcp::endpoint endpoint(address, port);

    // Return the parsed information
    return DiscoveryInfo {service_name, // Service name from the string
                          ip_str, port, endpoint};
}

} // namespace eestv