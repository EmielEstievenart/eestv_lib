#include "eestv/net/discovery/udp_discovery_client.hpp"
#include "boost/asio/post.hpp"
#include <algorithm>
#include <atomic>
#include <mutex>
#include "eestv/logging/eestv_logging.hpp"
#include "eestv/net/discovery/discovery_states.hpp"

namespace eestv
{

UdpDiscoveryClient::UdpDiscoveryClient(
    boost::asio::io_context& io_context, const std::string& service_name, std::chrono::milliseconds retry_timeout, int destination_port,
    std::function<bool(const std::string& response, const boost::asio::ip::udp::endpoint& remote_endpoint)> response_handler)
    : _io_context(io_context)
    , _service_name(service_name)
    , _retry_timeout(retry_timeout)
    , _destination_port(destination_port)
    , _on_response(std::move(response_handler))
    , _socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0))
    , _timer(io_context)
    , _recv_buffer()
{
    _socket.set_option(boost::asio::ip::multicast::outbound_interface(boost::asio::ip::address_v4::any()));
    _socket.set_option(boost::asio::socket_base::broadcast(true));
}

UdpDiscoveryClient::~UdpDiscoveryClient()
{
    stop();
};

bool UdpDiscoveryClient::async_start()
{
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_flags.get_flag(UdpDiscoveryState::running) || _flags.get_flag(UdpDiscoveryState::stopping))
        {
            return false;
        }

        _flags.set_flag(UdpDiscoveryState::running);
    }

    boost::asio::post(_io_context,
                      [this]()
                      {
                          std::unique_lock<std::mutex> my_lock(_mutex);
                          if (!_flags.get_flag(UdpDiscoveryState::stopping))
                          {
                              _flags.set_flag(UdpDiscoveryState::receiving_async);
                              _socket.async_receive_from(boost::asio::buffer(_recv_buffer), _remote_endpoint,
                                                         [this](const boost::system::error_code& error, std::size_t bytes_transferred)
                                                         { handle_response(error, bytes_transferred); });
                              send_discovery_request();
                          }
                          else
                          {
                              resolve_on_stopped();
                          }
                      });

    return true;
}

bool UdpDiscoveryClient::async_stop(std::function<void()> on_stopped)
{
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_flags.get_flag(UdpDiscoveryState::stopping))
        {
            return false;
        }

        _flags.set_flag(UdpDiscoveryState::stopping);
        _on_stopped = std::move(on_stopped);
    }

    //post a lambda to the context
    boost::asio::post(_io_context,
                      [this]()
                      {
                          std::unique_lock<std::mutex> my_lock(_mutex);
                          if (_flags.get_flag(UdpDiscoveryState::running))
                          {
                              _timer.cancel();
                              _socket.cancel();
                          }
                          else
                          {
                              if (_on_stopped)
                              {
                                  boost::asio::post(_io_context, std::move(_on_stopped));
                              }
                          }
                      });

    return true;
}

void UdpDiscoveryClient::stop()
{
    std::atomic_bool stopped = false;
    bool is_on_io_thread     = _io_context.get_executor().running_in_this_thread();
    if (is_on_io_thread)
    {
        EESTV_LOG_WARNING("Stop called on io context. This isn't allowed! ");
    }
    else if (async_stop([&stopped]() { stopped = true; }))
    {
        while (!stopped)
        {
        }
    }
}

void UdpDiscoveryClient::send_discovery_request()
{
    boost::asio::ip::udp::endpoint broadcast_endpoint(boost::asio::ip::make_address("255.255.255.255"), _destination_port);

    EESTV_LOG_TRACE("Sending discovery request: " << _service_name);
    _flags.set_flag(UdpDiscoveryState::sending_async);
    _socket.async_send_to(boost::asio::buffer(_service_name), broadcast_endpoint,
                          [this](const boost::system::error_code& error_code, std::size_t) { handle_send_complete(error_code); });
};

void UdpDiscoveryClient::handle_send_complete(const boost::system::error_code& error_code)
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (error_code)
    {
        if (error_code == boost::asio::error::operation_aborted)
        {
            EESTV_LOG_INFO("Discovery request sending aborted.");
        }
        else
        {
            EESTV_LOG_ERROR("Failed to send discovery request: " << error_code.message());
        }
    }
    else
    {
        if (!_flags.get_flag(UdpDiscoveryState::stopping))
        {
            _flags.set_flag(UdpDiscoveryState::timer_running);
            _timer.expires_after(_retry_timeout);
            _timer.async_wait([this](const boost::system::error_code& error_code) { handle_timeout(error_code); });
        }
    }
    _flags.clear_flag(UdpDiscoveryState::sending_async);
    resolve_on_stopped();
}

void UdpDiscoveryClient::handle_response(const boost::system::error_code& error_code, std::size_t bytes_transferred)
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (error_code)
    {
        if (error_code == boost::asio::error::operation_aborted)
        {
            EESTV_LOG_INFO("Discovery response receiving aborted.");
        }
        else
        {
            EESTV_LOG_ERROR("Error receiving discovery response: " << error_code.message());
        }
    }
    else if (bytes_transferred > 0)
    {
        // We received a response
        std::string response(_recv_buffer.data(), bytes_transferred);
        EESTV_LOG_TRACE("Discovery response from " << _remote_endpoint << ": " << response);
        if (_on_response)
        {
            // Call the response handler with the received response and remote endpoint
            if (_on_response(response, _remote_endpoint))
            {
                EESTV_LOG_DEBUG("Response handled successfully.");
            }
            else
            {
                EESTV_LOG_DEBUG("Response handling failed.");
            }
            _socket.async_receive_from(boost::asio::buffer(_recv_buffer), _remote_endpoint,
                                       [this](const boost::system::error_code& error, std::size_t bytes_transferred)
                                       { handle_response(error, bytes_transferred); });
        }
    }
    _flags.clear_flag(UdpDiscoveryState::receiving_async);
    resolve_on_stopped();
}

void UdpDiscoveryClient::handle_timeout(const boost::system::error_code& error_code)
{
    std::unique_lock<std::mutex> lock(_mutex);

    if (error_code)
    {
        if (error_code == boost::asio::error::operation_aborted)
        {
            EESTV_LOG_INFO("Timer operation aborted.");
        }
        else
        {

            EESTV_LOG_ERROR("Timer error: " << error_code.message());
        }
    }
    else
    {
        EESTV_LOG_INFO("Retry timeout expired, sending discovery request for: " << _service_name);
        send_discovery_request();
    }
    _flags.clear_flag(UdpDiscoveryState::timer_running);
    resolve_on_stopped();
}

void UdpDiscoveryClient::resolve_on_stopped()
{
    if (_flags.get_flag(UdpDiscoveryState::stopping))
    {
        if (!_flags.get_flag(UdpDiscoveryState::receiving_async) && !_flags.get_flag(UdpDiscoveryState::sending_async) &&
            !_flags.get_flag(UdpDiscoveryState::timer_running))
        {

            if (_on_stopped)
            {
                boost::asio::post(_io_context, std::move(_on_stopped));
            }
        }
    }
};

void UdpDiscoveryClient::reset()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _flags.clear_all();
};
} // namespace eestv