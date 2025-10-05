#include "udp_discovery_client.hpp"
#include <iostream>

UdpDiscoveryClient::UdpDiscoveryClient(boost::asio::io_context& io_context, const std::string& service_name,
                                       std::chrono::milliseconds retry_timeout, int port,
                                       std::function<bool(const std::string&, const boost::asio::ip::udp::endpoint&)> response_handler)
    : _io_context(io_context)
    , _service_name(service_name)
    , _retry_timeout(retry_timeout)
    , _port(port)
    , _response_handler(std::move(response_handler))
    , _socket(io_context, boost::asio::ip::udp::v4())
    , _timer(io_context)
    , _recv_buffer()
    , _running(false)
{
    _socket.set_option(boost::asio::ip::multicast::outbound_interface(boost::asio::ip::address_v4::any()));
    _socket.set_option(boost::asio::socket_base::broadcast(true));
}

void UdpDiscoveryClient::start()
{
    if (_running)
        return;

    _running = true;

    send_discovery_request();

    // Set up the retry timer
    _timer.expires_after(_retry_timeout);
    _timer.async_wait([this](const boost::system::error_code& error) { handle_timeout(error); });
}

void UdpDiscoveryClient::stop()
{
    _running = false;
    _timer.cancel();
    _socket.cancel();
}

void UdpDiscoveryClient::send_discovery_request()
{
    if (!_running)
        return;

    // Set up broadcast endpoint (assuming standard discovery port)

    boost::asio::ip::udp::endpoint broadcast_endpoint(boost::asio::ip::make_address("255.255.255.255"), _port);

    // Send the service name as the discovery request
    _socket.async_send_to(boost::asio::buffer(_service_name), broadcast_endpoint,
                          [this](const boost::system::error_code& error, std::size_t bytes_transferred)
                          {
                              if (error)
                              {
                                  std::cerr << "Failed to send discovery request: " << error.message() << std::endl;
                                  return;
                              }

                              // After sending, start listening for responses
                              _socket.async_receive_from(boost::asio::buffer(_recv_buffer), _remote_endpoint,
                                                         [this](const boost::system::error_code& error, std::size_t bytes_transferred)
                                                         { handle_response(error, bytes_transferred); });
                          });
}

void UdpDiscoveryClient::handle_response(const boost::system::error_code& error, std::size_t bytes_transferred)
{
    if (!_running)
        return;

    if (!error && bytes_transferred > 0)
    {
        // We received a response
        std::string response(_recv_buffer.data(), bytes_transferred);
        std::cout << "Discovery response from " << _remote_endpoint << ": " << response << std::endl;
        if (_response_handler)
        {
            // Call the response handler with the received response and remote endpoint
            if (_response_handler(response, _remote_endpoint))
            {
                std::cout << "Response handled successfully." << std::endl;
                _timer.cancel();
            }
            else
            {
                std::cout << "Response handling failed." << std::endl;
            }
        }

        // You might want to add a callback here to notify the application
        // about the discovered service and its response

        // Cancel the retry timer since we got a response

        // Optionally, you could stop here if you only need one response
        // or continue listening for more services
    }
    else if (error != boost::asio::error::operation_aborted)
    {
        std::cerr << "Error receiving discovery response: " << error.message() << std::endl;
    }
}

void UdpDiscoveryClient::handle_timeout(const boost::system::error_code& error)
{
    if (!_running || error == boost::asio::error::operation_aborted)
    {
        return;
    }

    if (error)
    {
        std::cerr << "Timer error: " << error.message() << std::endl;
        return;
    }

    // Retry timeout expired, send another discovery request
    std::cout << "Retry timeout expired, sending discovery request for: " << _service_name << std::endl;
    send_discovery_request();

    // Reset the timer for the next retry
    _timer.expires_after(_retry_timeout);
    _timer.async_wait([this](const boost::system::error_code& error) { handle_timeout(error); });
}