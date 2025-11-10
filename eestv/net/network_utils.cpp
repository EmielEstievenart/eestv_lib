#include "eestv/net/network_utils.hpp"
#include "eestv/logging/eestv_logging.hpp"
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>

namespace eestv
{

std::optional<std::string> get_local_address_for_remote(boost::asio::io_context& io_context,
                                                        const boost::asio::ip::udp::endpoint& remote_endpoint)
{
    try
    {
        // Create a UDP socket
        boost::asio::ip::udp::socket socket(io_context);
        boost::system::error_code ec;

        // Open the socket with the appropriate protocol version
        socket.open(remote_endpoint.protocol(), ec);
        if (ec)
        {
            EESTV_LOG_ERROR("Failed to open socket: " << ec.message());
            return std::nullopt;
        }

        // Connect the socket to the remote endpoint
        // For UDP, this doesn't actually send any packets, but tells the OS
        // which remote address we want to communicate with, allowing it to
        // select the appropriate local interface/address
        socket.connect(remote_endpoint, ec);
        if (ec)
        {
            EESTV_LOG_ERROR("Failed to connect socket to remote endpoint: " << ec.message());
            return std::nullopt;
        }

        // Get the local endpoint that was selected by the OS
        auto local_endpoint = socket.local_endpoint(ec);
        if (ec)
        {
            EESTV_LOG_ERROR("Failed to get local endpoint: " << ec.message());
            return std::nullopt;
        }

        // Extract and return the local IP address
        std::string local_address = local_endpoint.address().to_string();
        EESTV_LOG_DEBUG("Local address for remote " << remote_endpoint.address().to_string() << ": " << local_address);

        return local_address;
    }
    catch (const std::exception& e)
    {
        EESTV_LOG_ERROR("Exception in get_local_address_for_remote: " << e.what());
        return std::nullopt;
    }
}

} // namespace eestv
