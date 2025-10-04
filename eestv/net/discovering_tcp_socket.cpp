// DiscoveringTcpSocket.cpp
#include "eestv/net/discovering_tcp_socket.hpp"

DiscoveringTcpSocket::DiscoveringTcpSocket(boost::asio::io_context& io_context, const std::string& identifier, unsigned short udp_port,
                                           std::chrono::milliseconds retry_timeout)
    : boost::asio::ip::tcp::socket(io_context)
    , _identifier(identifier)
    , _udp_port(udp_port)
    , _io_context(io_context)
    , _retry_timeout(retry_timeout)
{
}

DiscoveringTcpSocket::~DiscoveringTcpSocket()
{
    cancel_discovery();
}

void DiscoveringTcpSocket::cancel_discovery()
{
    if (_discovery_client)
    {
        _discovery_client->stop();
        _discovery_client.reset();
    }
}