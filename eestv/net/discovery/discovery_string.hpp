#pragma once

#include <optional>
#include <string>

#include <boost/asio/ip/tcp.hpp>

namespace eestv
{

/**
 * @brief Parsed discovery response containing service information
 */
struct DiscoveryInfo
{
    std::string service_name;                ///< Name of the discovered service
    std::string ip_address;                  ///< IP address of the service
    unsigned short port;                     ///< Port number of the service
    boost::asio::ip::tcp::endpoint endpoint; ///< Full endpoint for convenience
};

/**
 * @brief Utility class for handling discovery string creation and parsing
 */
class DiscoveryString
{
public:
    /**
     * @brief Creates a discovery response string in "SERVICE_NAME:IP:PORT" format
     * @param service_name The name of the service
     * @param ip_address The IP address to include in the string
     * @param port The port number to include in the string
     * @return Discovery response string (e.g., "my_service:192.168.1.100:54321")
     */
    static std::string construct(const std::string& service_name, const std::string& ip_address, unsigned short port);

    /**
     * @brief Parses a discovery response string in "SERVICE_NAME:IP:PORT" format
     * @param response The response string to parse (e.g., "my_service:192.168.1.100:54321")
     * @return Parsed DiscoveryInfo on success, std::nullopt on failure
     */
    static std::optional<DiscoveryInfo> parse(const std::string& response);
};

} // namespace eestv