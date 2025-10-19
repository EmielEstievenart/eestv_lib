#include "eestv/net/discovery/udp_discovery_server.hpp"
#include <iostream>

namespace eestv
{

UdpDiscoveryServer::UdpDiscoveryServer(boost::asio::io_context& io_context, int port)
    : _io_context(io_context), _socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)), _port(port)
{
}

UdpDiscoveryServer::~UdpDiscoveryServer()
{
    if (_socket.is_open())
    {
        _socket.close();
    }
}

void UdpDiscoveryServer::start()
{
    start_receive();
}

void UdpDiscoveryServer::add_discoverable(const Discoverable& discoverable)
{
    std::lock_guard<std::mutex> lock(_discoverables_mutex);
    _discoverables.emplace(discoverable.get_identifier(), discoverable);
}

void UdpDiscoveryServer::start_receive()
{

    _socket.async_receive_from(boost::asio::buffer(_recv_buffer), _remote_endpoint,
                               [this](const boost::system::error_code& error, std::size_t bytes_transferred)
                               { handle_receive(error, bytes_transferred); });
}

void UdpDiscoveryServer::handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred)
{
    if (!error && bytes_transferred > 0)
    {
        std::string received_identifier(_recv_buffer.data(), bytes_transferred);

        // Look for the identifier in our discoverables
        std::string reply;
        {
            std::lock_guard<std::mutex> lock(_discoverables_mutex);
            auto it = _discoverables.find(received_identifier);
            if (it != _discoverables.end())
            {
                // Get the reply from the discoverable
                reply = it->second.get_reply();
            }
        }

        if (!reply.empty())
        {
            // Send the reply back to the requester
            auto send_buffer = std::make_shared<std::string>(reply);
            _socket.async_send_to(boost::asio::buffer(*send_buffer), _remote_endpoint,
                                  [this, send_buffer](const boost::system::error_code& error, std::size_t bytes_transferred)
                                  { handle_send(error, bytes_transferred); });
        }

        // Continue listening for more requests
        start_receive();
    }
    else
    {
        if (error != boost::asio::error::operation_aborted)
        {
            std::cerr << "UDP receive error: " << error.message() << std::endl;
            // Continue listening even after an error (unless it's operation_aborted)
            start_receive();
        }
    }
}

void UdpDiscoveryServer::handle_send(const boost::system::error_code& error, std::size_t bytes_transferred)
{
    if (error)
    {
        std::cerr << "UDP send error: " << error.message() << std::endl;
    }
    // Send operation completed, no further action needed
}
} // namespace eestv
