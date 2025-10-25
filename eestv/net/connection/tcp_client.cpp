#include "eestv/net/connection/tcp_client.hpp"

#include "boost/asio/connect.hpp"
#include "boost/asio/error.hpp"
#include "boost/asio/post.hpp"
#include "eestv/logging/eestv_logging.hpp"

#include <atomic>
#include <mutex>
#include <utility>

namespace eestv
{

TcpClient::TcpClient(boost::asio::io_context& io_context) : _io_context(io_context), _socket(io_context), _resolver(io_context)
{
}

TcpClient::~TcpClient()
{
    stop();
}

bool TcpClient::async_connect(const std::string& host, int port, ConnectionCallback on_connection_complete)
{
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_flags.get_flag(TcpClientState::resolving) || _flags.get_flag(TcpClientState::schedule_resolving) ||
            _flags.get_flag(TcpClientState::connecting) || _flags.get_flag(TcpClientState::stopping))
        {
            return false;
        }

        _flags.set_flag(TcpClientState::schedule_resolving);
        _on_connection_complete = std::move(on_connection_complete);
    }

    boost::asio::post(_io_context, [this, host, port]() { async_resolve(host, port); });

    return true;
}

void TcpClient::async_resolve(const std::string& host, int port)
{
    std::unique_lock<std::mutex> lock(_mutex);
    _flags.clear_flag(TcpClientState::schedule_resolving);
    if (_flags.get_flag(TcpClientState::stop_singaled))
    {
        resolve_on_stopped();
    }
    else
    {
        _flags.set_flag(TcpClientState::resolving);
        _resolver.async_resolve(host, std::to_string(port),
                                [this](const boost::system::error_code& error_code, boost::asio::ip::tcp::resolver::results_type results)
                                { handle_resolve(error_code, std::move(results)); });
    }
};

void TcpClient::handle_resolve(const boost::system::error_code& error_code, boost::asio::ip::tcp::resolver::results_type results)
{
    std::unique_lock<std::mutex> lock(_mutex, std::defer_lock);

    if (error_code)
    {
        if (error_code == boost::asio::error::operation_aborted)
        {
            EESTV_LOG_INFO("Resolve operation aborted by user");
        }
        else
        {
            EESTV_LOG_ERROR("Failed to resolve host - " << error_code.message());
        }
        lock.lock();
        if (_on_connection_complete)
        {
            auto callback = std::move(_on_connection_complete);
            boost::asio::post(_io_context,
                              [this, error_code, callback = std::move(callback)]() mutable
                              {
                                  if (callback)
                                  {
                                      callback(boost::asio::ip::tcp::socket(this->_io_context), error_code);
                                  }
                              });
        }
    }
    else
    {
        lock.lock();
        _flags.set_flag(TcpClientState::connecting);

        boost::asio::async_connect(_socket, results,
                                   [this](const boost::system::error_code& error_code, const boost::asio::ip::tcp::endpoint&)
                                   { handle_connect(error_code); });
    }
    _flags.clear_flag(TcpClientState::resolving);

    if (_flags.get_flag(TcpClientState::stop_singaled))
    {
        resolve_on_stopped();
    }
};

bool TcpClient::async_stop(std::function<void()> on_stopped)
{
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_flags.get_flag(TcpClientState::stop_singaled))
        {
            return false;
        }

        _flags.set_flag(TcpClientState::stop_singaled);
        _flags.set_flag(TcpClientState::stopping);
        _on_stopped = std::move(on_stopped);
    }

    boost::asio::post(_io_context,
                      [this]()
                      {
                          std::unique_lock<std::mutex> lock(_mutex);

                          boost::system::error_code error_code;
                          _socket.cancel(error_code);
                          _resolver.cancel();
                          _flags.clear_flag(TcpClientState::stopping);
                          resolve_on_stopped();
                      });

    return true;
}

void TcpClient::stop()
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

void TcpClient::reset()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _flags.clear_all();
}

std::string TcpClient::to_string() const
{
    std::unique_lock<std::mutex> lock(_mutex);
    std::string result = "TcpClient flags: [";
    bool first         = true;

    if (_flags.get_flag(TcpClientState::schedule_resolving))
    {
        if (!first)
        {
            result += ", ";
        }
        result += eestv::to_string(TcpClientState::schedule_resolving);
        first = false;
    }
    if (_flags.get_flag(TcpClientState::resolving))
    {
        if (!first)
        {
            result += ", ";
        }
        result += eestv::to_string(TcpClientState::resolving);
        first = false;
    }
    if (_flags.get_flag(TcpClientState::connecting))
    {
        if (!first)
        {
            result += ", ";
        }
        result += eestv::to_string(TcpClientState::connecting);
        first = false;
    }
    if (_flags.get_flag(TcpClientState::stopping))
    {
        if (!first)
        {
            result += ", ";
        }
        result += eestv::to_string(TcpClientState::stopping);
        first = false;
    }
    if (_flags.get_flag(TcpClientState::stop_singaled))
    {
        if (!first)
        {
            result += ", ";
        }
        result += eestv::to_string(TcpClientState::stop_singaled);
        first = false;
    }

    result += "]";
    return result;
}

void TcpClient::handle_connect(const boost::system::error_code& error_code)
{
    std::unique_lock<std::mutex> lock(_mutex, std::defer_lock);

    if (error_code)
    {
        if (error_code == boost::asio::error::operation_aborted)
        {
            EESTV_LOG_INFO("Connection attempt aborted by user");
        }
        else
        {
            EESTV_LOG_ERROR("Connection failed: " << error_code.message());
        }
        lock.lock();
        _flags.clear_flag(TcpClientState::connecting);
        if (_flags.get_flag(TcpClientState::stop_singaled))
        {
            resolve_on_stopped();
        }
        if (_on_connection_complete)
        {
            auto callback = std::move(_on_connection_complete);
            boost::asio::post(_io_context,
                              [this, error_code, callback = std::move(callback)]() mutable
                              {
                                  if (callback)
                                  {
                                      callback(boost::asio::ip::tcp::socket(this->_io_context), error_code);
                                  }
                              });
        }
    }
    else
    {
        lock.lock();
        _flags.clear_flag(TcpClientState::connecting);
        if (_flags.get_flag(TcpClientState::stop_singaled))
        {
            resolve_on_stopped();
        }
        else if (_on_connection_complete)
        {
            auto callback = std::move(_on_connection_complete);
            auto socket   = std::move(_socket);
            lock.unlock();
            if (callback)
            {
                callback(std::move(socket), error_code);
            }
        }
    }
}

void TcpClient::resolve_on_stopped()
{
    if (!_flags.get_flag(TcpClientState::stopping) && !_flags.get_flag(TcpClientState::schedule_resolving) &&
        !_flags.get_flag(TcpClientState::resolving) && !_flags.get_flag(TcpClientState::connecting))
    {
        auto stopped_callback = std::move(_on_stopped);
        _on_stopped           = nullptr;

        if (stopped_callback)
        {
            boost::asio::post(_io_context,
                              [stopped_callback = std::move(stopped_callback)]() mutable
                              {
                                  if (stopped_callback)
                                  {
                                      stopped_callback();
                                  }
                              });
        }
    }
}

} // namespace eestv
