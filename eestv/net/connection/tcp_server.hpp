#pragma once

#include "eestv/net/connection/tcp_connection.hpp"
#include "eestv/logging/eestv_logging.hpp"
#include "eestv/net/connection/tcp_server_states.hpp"
#include "eestv/flags/flags.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace eestv
{

template <typename ReceiveBuffer = LinearBuffer, typename SendBuffer = LinearBuffer>
class TcpServer
{
public:
    using ConnectionPtr      = std::unique_ptr<TcpConnection<ReceiveBuffer, SendBuffer>>;
    using ConnectionCallback = std::function<void(ConnectionPtr)>;
    using StoppedCallback    = std::function<void()>;

    static constexpr std::size_t default_buffer_size = 4096;

    /**
     * @brief Construct a TCP server
     * 
     * @param io_context The Boost.Asio io_context to use
     * @param port The port to listen on
     * @param receive_buffer_size Size of receive buffer for each connection
     * @param send_buffer_size Size of send buffer for each connection
     */
    TcpServer(boost::asio::io_context& io_context, unsigned short port, std::size_t receive_buffer_size = default_buffer_size,
              std::size_t send_buffer_size = default_buffer_size);

    /**
     * @brief Construct a TCP server with specific endpoint
     * 
     * @param io_context The Boost.Asio io_context to use
     * @param endpoint The endpoint to bind to (address + port)
     * @param receive_buffer_size Size of receive buffer for each connection
     * @param send_buffer_size Size of send buffer for each connection
     */
    TcpServer(boost::asio::io_context& io_context, const boost::asio::ip::tcp::endpoint& endpoint,
              std::size_t receive_buffer_size = default_buffer_size, std::size_t send_buffer_size = default_buffer_size);

    ~TcpServer();

    TcpServer(const TcpServer&)            = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&)                 = delete;
    TcpServer& operator=(TcpServer&&)      = delete;

    void set_connection_callback(ConnectionCallback callback)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _connection_callback = std::move(callback);
    }

    bool async_start();

    bool async_stop(StoppedCallback on_stopped);

    void stop();

    bool is_running() const { return get_flag_thread_safe(TcpServerState::accepting); }

    boost::asio::ip::tcp::endpoint local_endpoint() const { return _acceptor.local_endpoint(); }

    unsigned short port() const { return _acceptor.local_endpoint().port(); }

private:
    mutable std::mutex _mutex;

    void set_flag_thread_safe(TcpServerState state)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _flags.set_flag(state);
    }

    void clear_flag_thread_safe(TcpServerState state)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _flags.clear_flag(state);
    }

    bool get_flag_thread_safe(TcpServerState state) const
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _flags.get_flag(state);
    }

    // Internal methods that run on io_context thread

    void async_accept();
    void handle_accept(const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket);
    void resolve_on_stopped();

    boost::asio::io_context& _io_context;
    boost::asio::ip::tcp::acceptor _acceptor;
    boost::asio::ip::tcp::endpoint _cached_local_endpoint; // Cache endpoint to safely access after close
    std::size_t _receive_buffer_size;
    std::size_t _send_buffer_size;
    ConnectionCallback _connection_callback;
    StoppedCallback _stopped_callback;

    Flags<TcpServerState> _flags;
};

// Template implementation

template <typename ReceiveBuffer, typename SendBuffer>
TcpServer<ReceiveBuffer, SendBuffer>::TcpServer(boost::asio::io_context& io_context, unsigned short port, std::size_t receive_buffer_size,
                                                std::size_t send_buffer_size)
    : _io_context(io_context)
    , _acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    , _cached_local_endpoint(_acceptor.local_endpoint())
    , _receive_buffer_size(receive_buffer_size)
    , _send_buffer_size(send_buffer_size)
{
}

template <typename ReceiveBuffer, typename SendBuffer>
TcpServer<ReceiveBuffer, SendBuffer>::TcpServer(boost::asio::io_context& io_context, const boost::asio::ip::tcp::endpoint& endpoint,
                                                std::size_t receive_buffer_size, std::size_t send_buffer_size)
    : _io_context(io_context)
    , _acceptor(io_context, endpoint)
    , _cached_local_endpoint(_acceptor.local_endpoint())
    , _receive_buffer_size(receive_buffer_size)
    , _send_buffer_size(send_buffer_size)
{
}

template <typename ReceiveBuffer, typename SendBuffer>
TcpServer<ReceiveBuffer, SendBuffer>::~TcpServer()
{
    stop();
}

template <typename ReceiveBuffer, typename SendBuffer>
bool TcpServer<ReceiveBuffer, SendBuffer>::async_start()
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (_flags.get_flag(TcpServerState::start_signaled) || _flags.get_flag(TcpServerState::stopping))
    {
        return false;
    }
    _flags.set_flag(TcpServerState::start_signaled);
    _flags.set_flag(TcpServerState::start_accepting);
    boost::asio::post(_io_context,
                      [this]()
                      {
                          std::unique_lock<std::mutex> lock(_mutex);
                          this->async_accept();
                      });

    return true;
}

template <typename ReceiveBuffer, typename SendBuffer>
bool TcpServer<ReceiveBuffer, SendBuffer>::async_stop(StoppedCallback on_stopped)
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (_flags.get_flag(TcpServerState::stop_signaled) || !_flags.get_flag(TcpServerState::start_signaled))
    {
        return false;
    }
    _flags.set_flag(TcpServerState::stop_signaled);
    _stopped_callback = std::move(on_stopped);

    _flags.set_flag(TcpServerState::stopping);
    boost::asio::post(_io_context,
                      [this]()
                      {
                          std::unique_lock<std::mutex> lock(_mutex);
                          _flags.set_flag(TcpServerState::stopping);

                          boost::system::error_code error_code;
                          if (_acceptor.is_open())
                          {
                              // Even though the docs say it will cancel immediately, that is not true. It cancels immediately **on the io_context's thread**
                              //   _acceptor.close(error_code);
                              _acceptor.cancel(error_code);
                          }
                          else
                          {
                              resolve_on_stopped();
                          }
                      });
    return true;
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::stop()
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

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::async_accept()
{
    _flags.clear_flag(TcpServerState::start_accepting);
    if (!_flags.get_flag(TcpServerState::stopping))
    {
        _flags.set_flag(TcpServerState::accepting);
        _acceptor.async_accept([this](const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket)
                               { handle_accept(error_code, std::move(socket)); });
    }
    else
    {
        resolve_on_stopped();
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::handle_accept(const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket)
{
    std::unique_lock<std::mutex> lock(_mutex);

    if (error_code)
    {
        if (error_code == boost::asio::error::operation_aborted)
        {
            EESTV_LOG_INFO("Server stopped with operation_aborted on endpoint " << _cached_local_endpoint.address().to_string() << ":"
                                                                                << _cached_local_endpoint.port());
        }
        else
        {
            EESTV_LOG_ERROR("Accept operation failed " << error_code.message() << " (code=" << error_code.value()
                                                       << ", category=" << error_code.category().name() << ") "
                                                       << "on endpoint " << _cached_local_endpoint.address().to_string() << " : "
                                                       << _cached_local_endpoint.port());
        }
        _flags.clear_flag(TcpServerState::accepting);
        resolve_on_stopped();
    }
    else
    {
        auto connection = std::make_unique<TcpConnection<ReceiveBuffer, SendBuffer>>(std::move(socket), _io_context, _receive_buffer_size,
                                                                                     _send_buffer_size);
        if (_connection_callback)
        {
            _connection_callback(std::move(connection));
        }

        if (!_flags.get_flag(TcpServerState::stopping))
        {
            async_accept();
        }
        else
        {
            _flags.clear_flag(TcpServerState::accepting);
            resolve_on_stopped();
        }
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::resolve_on_stopped()
{
    if (!_flags.get_flag(TcpServerState::start_accepting) && !_flags.get_flag(TcpServerState::accepting))
    {
        // Don't clear the closing flag - server cannot be restarted after being stopped
        // because the acceptor is closed and can't be reopened
        if (_stopped_callback)
        {
            auto callback     = std::move(_stopped_callback);
            _stopped_callback = nullptr;
            boost::asio::post(_io_context, std::move(callback));
        }
    }
}

} // namespace eestv
