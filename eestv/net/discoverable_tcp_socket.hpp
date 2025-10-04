// DiscoverableTcpSocket.hpp
#pragma once

#include "eestv/net/udp_discovery_server.hpp"
#include "eestv/net/discoverable.hpp"
#include <boost/asio.hpp>
#include <string>
#include <memory>

class DiscoverableTcpSocket
{
private:
    boost::asio::io_context& _io_context;
    std::string _identifier;
    unsigned short _udp_port;
    unsigned short _tcp_port;
    boost::asio::ip::tcp::acceptor _acceptor;

    // UDP discovery components
    std::unique_ptr<UdpDiscoveryServer> _discovery_server;
    std::unique_ptr<Discoverable> _discoverable_service;

public:
    /**
   * @brief Constructs a discoverable TCP socket.
   * @param io_context The IO context for async operations.
   * @param identifier The service identifier for discovery.
   * @param udp_port The UDP port for discovery requests.
   * @param tcp_port The TCP port for connections (0 for auto-assign).
   */
    DiscoverableTcpSocket(boost::asio::io_context& io_context, const std::string& identifier, unsigned short udp_port,
                          unsigned short tcp_port = 0);

    /**
   * @brief Destructor - cleans up resources.
   */
    ~DiscoverableTcpSocket();

    /**
   * @brief Starts the discovery service.
   */
    void start();

    /**
   * @brief Asynchronously accepts an incoming TCP connection.
   * @param socket The socket to accept the connection into.
   * @param handler Completion handler to be called when the accept operation completes.
   *               Handler signature: void(boost::system::error_code ec)
   */
    template <typename AcceptHandler> void async_accept(boost::asio::ip::tcp::socket& socket, AcceptHandler&& handler);

    /**
   * @brief Gets the TCP port this socket is listening on.
   * @return The TCP port number.
   */
    unsigned short get_tcp_port() const { return _tcp_port; }

    /**
   * @brief Gets the UDP discovery port.
   * @return The UDP port number.
   */
    unsigned short get_udp_port() const { return _udp_port; }
};

// Template method implementation
template <typename AcceptHandler> void DiscoverableTcpSocket::async_accept(boost::asio::ip::tcp::socket& socket, AcceptHandler&& handler)
{
    _acceptor.async_accept(socket, std::forward<AcceptHandler>(handler));
}