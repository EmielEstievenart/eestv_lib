// DiscoverableTcpSocket.cpp
#include "eestv/net/discoverable_tcp_socket.hpp"
#include <boost/system/system_error.hpp>

DiscoverableTcpSocket::DiscoverableTcpSocket(boost::asio::io_context& io_context, const std::string& identifier, unsigned short udp_port,
                                             unsigned short tcp_port)
    : _io_context(io_context)
    , _identifier(identifier)
    , _udp_port(udp_port)
    , _tcp_port(tcp_port)
    , _acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), _tcp_port))
{

    // Get the actual TCP port if auto-assigned
    if (_tcp_port == 0)
    {
        _tcp_port = _acceptor.local_endpoint().port();
    }

    // Create the UDP discovery server
    _discovery_server = std::make_unique<UdpDiscoveryServer>(io_context, _udp_port);

    // Create the discoverable service that returns the TCP port
    _discoverable_service = std::make_unique<Discoverable>(_identifier, [this]() -> std::string { return std::to_string(_tcp_port); });

    // Register the service with the discovery server
    _discovery_server->add_discoverable(*_discoverable_service);
}

DiscoverableTcpSocket::~DiscoverableTcpSocket()
{
    // The unique_ptr destructors will clean up the discovery server and service
    if (_acceptor.is_open())
    {
        _acceptor.close();
    }
}

void DiscoverableTcpSocket::start()
{
    // Start the UDP discovery server
    _discovery_server->start();
}