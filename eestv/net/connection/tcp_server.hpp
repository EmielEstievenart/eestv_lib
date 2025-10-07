#pragma once

#include "eestv/net/connection/tcp_server_connection.hpp"
#include "eestv/logging/eestv_logging.hpp"

#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <mutex>

namespace eestv
{

/**
 * @brief TCP server that accepts connections and creates TcpServerConnection instances
 * 
 * The TcpServer listens on a specified port and automatically accepts incoming
 * connections. Each accepted connection is wrapped in a TcpServerConnection and
 * passed to a user-provided callback.
 * 
 * @tparam ReceiveBuffer The buffer type to use for receiving data in TcpServerConnection instances
 * @tparam SendBuffer The buffer type to use for sending data in TcpServerConnection instances
 */
template <typename ReceiveBuffer = LinearBuffer, typename SendBuffer = LinearBuffer>
class TcpServer
{
public:
    using ConnectionPtr      = std::shared_ptr<TcpServerConnection<ReceiveBuffer, SendBuffer>>;
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
     * @param keepalive_interval Keepalive interval for accepted connections
     */
    TcpServer(boost::asio::io_context& io_context, unsigned short port, std::size_t receive_buffer_size = default_buffer_size,
              std::size_t send_buffer_size            = default_buffer_size,
              std::chrono::seconds keepalive_interval = TcpServerConnection<ReceiveBuffer, SendBuffer>::default_keepalive_interval);

    /**
     * @brief Construct a TCP server with specific endpoint
     * 
     * @param io_context The Boost.Asio io_context to use
     * @param endpoint The endpoint to bind to (address + port)
     * @param receive_buffer_size Size of receive buffer for each connection
     * @param send_buffer_size Size of send buffer for each connection
     * @param keepalive_interval Keepalive interval for accepted connections
     */
    TcpServer(boost::asio::io_context& io_context, const boost::asio::ip::tcp::endpoint& endpoint,
              std::size_t receive_buffer_size = default_buffer_size, std::size_t send_buffer_size = default_buffer_size,
              std::chrono::seconds keepalive_interval = TcpServerConnection<ReceiveBuffer, SendBuffer>::default_keepalive_interval);

    ~TcpServer() = default;

    TcpServer(const TcpServer&)            = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&)                 = delete;
    TcpServer& operator=(TcpServer&&)      = delete;

    /**
     * @brief Set the callback for new connections
     * 
     * This callback is invoked whenever a new client connects.
     * 
     * @param callback Function to call with each new TcpServerConnection
     */
    void set_connection_callback(ConnectionCallback callback)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _connection_callback = std::move(callback);
    }

    /**
     * @brief Start accepting connections
     * 
     * Begins listening for and accepting new connections.
     */
    void async_start();

    /**
     * @brief Stop accepting connections
     * 
     * Stops the acceptor and closes any pending accept operations.
     * 
     * @param callback Optional callback to be invoked when the server has fully stopped
     */
    void async_stop(StoppedCallback callback = nullptr);

    /**
     * @brief Check if the server is currently accepting connections
     * 
     * @return true if accepting, false otherwise
     */
    bool is_running() const { return _is_running; }

    /**
     * @brief Get the local endpoint the server is bound to
     * 
     * @return The local endpoint (address and port)
     */
    boost::asio::ip::tcp::endpoint local_endpoint() const { return _acceptor.local_endpoint(); }

    /**
     * @brief Get the port the server is listening on
     * 
     * @return The port number
     */
    unsigned short port() const { return _acceptor.local_endpoint().port(); }

private:
    void async_start_accept();
    void handle_accept(const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket);

    boost::asio::io_context& _io_context;
    boost::asio::ip::tcp::acceptor _acceptor;
    std::size_t _receive_buffer_size;
    std::size_t _send_buffer_size;
    std::chrono::seconds _keepalive_interval;
    ConnectionCallback _connection_callback;
    StoppedCallback _pending_stopped_callback;

    bool _is_running;
    std::mutex _mutex;
};

// Template implementation

template <typename ReceiveBuffer, typename SendBuffer>
TcpServer<ReceiveBuffer, SendBuffer>::TcpServer(boost::asio::io_context& io_context, unsigned short port, std::size_t receive_buffer_size,
                                                std::size_t send_buffer_size, std::chrono::seconds keepalive_interval)
    : _io_context(io_context)
    , _acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    , _receive_buffer_size(receive_buffer_size)
    , _send_buffer_size(send_buffer_size)
    , _keepalive_interval(keepalive_interval)
    , _is_running(false)
{
}

template <typename ReceiveBuffer, typename SendBuffer>
TcpServer<ReceiveBuffer, SendBuffer>::TcpServer(boost::asio::io_context& io_context, const boost::asio::ip::tcp::endpoint& endpoint,
                                                std::size_t receive_buffer_size, std::size_t send_buffer_size,
                                                std::chrono::seconds keepalive_interval)
    : _io_context(io_context)
    , _acceptor(io_context, endpoint)
    , _receive_buffer_size(receive_buffer_size)
    , _send_buffer_size(send_buffer_size)
    , _keepalive_interval(keepalive_interval)
    , _is_running(false)
{
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::async_start()
{
    std::unique_lock<std::mutex> lock(_mutex);

    if (_is_running)
    {
        return;
    }

    _is_running = true;
    async_start_accept();
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::async_stop(StoppedCallback callback)
{
    std::unique_lock<std::mutex> lock(_mutex);

    if (!_is_running)
    {
        return;
    }

    _pending_stopped_callback = std::move(callback);

    boost::system::error_code error_code;
    //Even though the docs say it will cancel immediately, that is not true. It cancels immediately **on the io_context's thread**
    _acceptor.close(error_code);
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::async_start_accept()
{
    _acceptor.async_accept([this](const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket)
                           { handle_accept(error_code, std::move(socket)); });
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::handle_accept(const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket)
{
    std::unique_lock<std::mutex> lock(_mutex);

    if (error_code)
    {
        if (error_code == boost::asio::error::operation_aborted)
        {
            EESTV_LOG_INFO("Server stopped with operation_aborted on endpoint " << _acceptor.local_endpoint().address().to_string() << ":"
                                                                                << _acceptor.local_endpoint().port());
        }
        else
        {
            EESTV_LOG_ERROR("Accept operation failed " << error_code.message() << " (code=" << error_code.value()
                                                       << ", category=" << error_code.category().name() << ") "
                                                       << "on endpoint " << _acceptor.local_endpoint().address().to_string() << " : "
                                                       << _acceptor.local_endpoint().port());
        }

        _is_running = false;

        // Post stopped callback to io_context for immediate execution
        if (_pending_stopped_callback)
        {
            boost::asio::post(_io_context,
                              [callback = std::move(_pending_stopped_callback)]()
                              {
                                  EESTV_LOG_INFO("The server has stopped. ");
                                  callback();
                              });
            _pending_stopped_callback = nullptr;
        }

        return;
    }

    auto connection = std::make_shared<TcpServerConnection<ReceiveBuffer, SendBuffer>>(std::move(socket), _io_context, _receive_buffer_size,
                                                                                       _send_buffer_size, _keepalive_interval);
    connection->start_alive_monitoring();
    if (_connection_callback)
    {
        _connection_callback(connection);
    }

    async_start_accept();
}

} // namespace eestv
