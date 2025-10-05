#pragma once

#include "discoverable.hpp"
#include "boost/asio.hpp"
#include "boost/asio/io_context.hpp"
#include <unordered_map>
#include <array>
#include <mutex>

class UdpDiscoveryServer
{

public:
    /**
     * @brief Constructs a UdpDiscoveryServer.
     * @param io_context The Boost.Asio IO context for async operations.
     * @param port The UDP port to listen on for discovery requests.
     */
    UdpDiscoveryServer(boost::asio::io_context& io_context, int port);

    /**
     * @brief Destroys the UdpDiscoveryServer and closes the UDP socket.
     */
    ~UdpDiscoveryServer();

    /**
     * @brief Starts the discovery server to listen for incoming requests.
     * 
     * This method begins asynchronous listening for UDP packets. The server
     * will continue to handle requests until the io_context is stopped or
     * the server is destroyed.
     */
    void start();

    /**
     * @brief Registers a discoverable service with the server.
     * @param discoverable The Discoverable object to register. Its identifier
     *                    will be used as the lookup key for discovery requests.
     * 
     * Note: If a service with the same identifier already exists, it will be
     * replaced with the new one.
     */
    void add_discoverable(const Discoverable& discoverable);

private:
    /**
     * @brief Initiates an asynchronous receive operation.
     */
    void start_receive();

    /**
     * @brief Handles received UDP data and processes discovery requests.
     * @param error Error code from the receive operation.
     * @param bytes_transferred Number of bytes received.
     */
    void handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred);

    /**
     * @brief Handles completion of send operations.
     * @param error Error code from the send operation.
     * @param bytes_transferred Number of bytes sent.
     */
    void handle_send(const boost::system::error_code& error, std::size_t bytes_transferred);

    boost::asio::io_context& _io_context;
    boost::asio::ip::udp::socket _socket;
    boost::asio::ip::udp::endpoint _remote_endpoint;
    /// Size of the receive buffer in bytes.
    static constexpr std::size_t recv_buffer_size = 1024;
    std::array<char, recv_buffer_size> _recv_buffer;
    std::unordered_map<std::string, Discoverable> _discoverables;
    mutable std::mutex _discoverables_mutex; // Add mutex for thread safety
    int _port;
};
