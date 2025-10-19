#include "eestv/net/discovery/udp_discovery_server.hpp"
#include "boost/asio/post.hpp"
#include "eestv/logging/eestv_logging.hpp"
#include "eestv/net/discovery/discovery_states.hpp"
#include <atomic>

namespace eestv
{

UdpDiscoveryServer::UdpDiscoveryServer(boost::asio::io_context& io_context, int port)
    : _io_context(io_context)
    , _socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port))
    , _port(port)
    , _recv_buffer()
{
}

UdpDiscoveryServer::~UdpDiscoveryServer()
{
    stop();
}

bool UdpDiscoveryServer::async_start()
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
                              start_receive();
                          }
                          else
                          {
                              resolve_on_stopped();
                          }
                      });

    return true;
}

bool UdpDiscoveryServer::async_stop(std::function<void()> on_stopped)
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

    boost::asio::post(_io_context,
                      [this]()
                      {
                          std::unique_lock<std::mutex> my_lock(_mutex);
                          if (_flags.get_flag(UdpDiscoveryState::running))
                          {
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

void UdpDiscoveryServer::stop()
{
    std::atomic_bool stopped = false;
    if (async_stop([&stopped]() { stopped = true; }))
    {
        while (!stopped)
        {
        }
    }
}

void UdpDiscoveryServer::add_discoverable(const Discoverable& discoverable)
{
    std::lock_guard<std::mutex> lock(_discoverables_mutex);
    _discoverables.emplace(discoverable.get_identifier(), discoverable);
}

void UdpDiscoveryServer::start_receive()
{
    _flags.set_flag(UdpDiscoveryState::receiving_async);
    _socket.async_receive_from(boost::asio::buffer(_recv_buffer), _remote_endpoint,
                               [this](const boost::system::error_code& error_code, std::size_t bytes_transferred)
                               { handle_receive(error_code, bytes_transferred); });
}

void UdpDiscoveryServer::handle_receive(const boost::system::error_code& error_code, std::size_t bytes_transferred)
{
    std::unique_lock<std::mutex> lock(_mutex);

    if (error_code)
    {
        if (error_code == boost::asio::error::operation_aborted)
        {
            EESTV_LOG_INFO("Discovery server receive operation aborted.");
        }
        else
        {
            EESTV_LOG_ERROR("UDP receive error: " << error_code.message());
        }
    }
    else if (bytes_transferred > 0)
    {
        std::string received_identifier(_recv_buffer.data(), bytes_transferred);

        // Look for the identifier in our discoverables
        std::string reply;
        {
            std::lock_guard<std::mutex> disc_lock(_discoverables_mutex);
            auto iter = _discoverables.find(received_identifier);
            if (iter != _discoverables.end())
            {
                // Get the reply from the discoverable
                reply = iter->second.get_reply();
            }
        }

        if (!reply.empty())
        {
            auto send_buffer = std::make_shared<std::string>(reply);
            _flags.set_flag(UdpDiscoveryState::sending_async);
            _socket.async_send_to(boost::asio::buffer(*send_buffer), _remote_endpoint,
                                  [this, send_buffer](const boost::system::error_code& error_code, std::size_t bytes_transferred)
                                  { handle_send(error_code, bytes_transferred); });
        }
    }

    if (!_flags.get_flag(UdpDiscoveryState::stopping))
    {
        _socket.async_receive_from(boost::asio::buffer(_recv_buffer), _remote_endpoint,
                                   [this](const boost::system::error_code& error, std::size_t bytes_transferred)
                                   { handle_receive(error, bytes_transferred); });
        return;
    }
    else
    {
        _flags.clear_flag(UdpDiscoveryState::receiving_async);
    }
    resolve_on_stopped();
}

void UdpDiscoveryServer::handle_send(const boost::system::error_code& error, std::size_t /*bytes_transferred*/)
{
    std::unique_lock<std::mutex> lock(_mutex);

    if (error)
    {
        if (error == boost::asio::error::operation_aborted)
        {
            EESTV_LOG_INFO("Discovery server send operation aborted.");
        }
        else
        {
            EESTV_LOG_ERROR("UDP send error: " << error.message());
        }
    }

    _flags.clear_flag(UdpDiscoveryState::sending_async);
    resolve_on_stopped();
}

void UdpDiscoveryServer::resolve_on_stopped()
{
    if (_flags.get_flag(UdpDiscoveryState::stopping))
    {
        if (!_flags.get_flag(UdpDiscoveryState::receiving_async) && !_flags.get_flag(UdpDiscoveryState::sending_async))
        {
            if (_on_stopped)
            {
                boost::asio::post(_io_context, std::move(_on_stopped));
            }
        }
    }
}

void UdpDiscoveryServer::reset()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _flags.clear_all();
}

} // namespace eestv
