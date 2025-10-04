// DiscoveringTcpSocket.hpp
#pragma once

#include "eestv/net/udp_discovery_client.hpp"
#include <boost/asio.hpp>
#include <string>
#include <memory>
#include <chrono>

namespace ba = boost::asio;
using boost::asio::ip::tcp;

/**
 * @brief A discovering TCP socket that uses UdpDiscoveryClient to find services.
 * 
 * This class uses UDP-based service discovery to find and connect to
 * discoverable TCP services. It sends discovery requests and connects
 * to the TCP port returned by the discovered service.
 * 
 * Usage Example:
 * @code
 * boost::asio::io_context io_context;
 * 
 * // Create a discovering socket
 * DiscoveringTcpSocket socket(io_context, "my_service", 12345);
 * 
 * // Asynchronously connect to the discovered service
 * socket.async_connect_via_discovery([](boost::system::error_code ec) {
 *     if (!ec) {
 *         std::cout << "Connected to service!" << std::endl;
 *         // Now use it as a regular TCP socket
 *         // socket.write_some(boost::asio::buffer("Hello"));
 *     } else {
 *         std::cerr << "Connection failed: " << ec.message() << std::endl;
 *     }
 * });
 * 
 * // Run the IO context to handle async operations
 * io_context.run();
 * @endcode
 */
class DiscoveringTcpSocket : public tcp::socket
{
private:
    std::string _identifier;
    unsigned short _udp_port;
    ba::io_context& _io_context;
    std::unique_ptr<UdpDiscoveryClient> _discovery_client;
    std::chrono::milliseconds _retry_timeout;

public:
    /**
     * @brief Constructs a discovering TCP socket.
     * @param io_context The IO context for async operations.
     * @param identifier The service identifier to discover.
     * @param udp_port The UDP port for discovery requests.
     * @param retry_timeout The timeout between discovery retries (default: 1 second).
     */
    DiscoveringTcpSocket(boost::asio::io_context& io_context, const std::string& identifier, unsigned short udp_port,
                         std::chrono::milliseconds retry_timeout = std::chrono::seconds(1));

    /**
     * @brief Asynchronously connects to a discovered TCP service.
     * @param handler Completion handler called when connection completes or fails.
     *               Handler signature: void(boost::system::error_code ec)
     */
    template <typename CompletionHandler> void async_connect_via_discovery(CompletionHandler&& handler);

    /**
     * @brief Destructor - ensures proper cleanup of discovery client.
     */
    ~DiscoveringTcpSocket();

    /**
     * @brief Cancels any ongoing discovery operation.
     */
    void cancel_discovery();
};

// Template method implementation
template <typename CompletionHandler> void DiscoveringTcpSocket::async_connect_via_discovery(CompletionHandler&& handler)
{
    // Create the discovery client with a response handler
    _discovery_client = std::make_unique<UdpDiscoveryClient>(
        _io_context, _identifier, _retry_timeout, _udp_port,
        [this, handler = std::forward<CompletionHandler>(handler)](
            const std::string& response, const boost::asio::ip::udp::endpoint& remote_endpoint) mutable -> bool
        {
            try
            {
                // Parse the discovered port
                unsigned short discovered_port = static_cast<unsigned short>(std::stoul(response));

                // Stop the discovery client
                _discovery_client->stop();

                // Connect to the discovered service using the remote endpoint's address
                tcp::endpoint service_endpoint(remote_endpoint.address(), discovered_port);

                // Perform async TCP connection
                this->async_connect(service_endpoint,
                                   [handler = std::move(handler)](boost::system::error_code ec) mutable
                                   {
                                       // Call the completion handler with the result
                                       handler(ec);
                                   });

                return true; // Stop discovery after first response
            }
            catch (const std::exception& e)
            {
                // Invalid response, continue discovery
                return false;
            }
        });

    // Start the discovery process
    _discovery_client->start();
}