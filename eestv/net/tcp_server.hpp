#pragma once

#include "eestv/net/server_connection.hpp"
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace eestv
{

/**
 * @brief TCP server that accepts connections and creates ServerConnection instances
 * 
 * The TcpServer listens on a specified port and automatically accepts incoming
 * connections. Each accepted connection is wrapped in a ServerConnection and
 * passed to a user-provided callback.
 * 
 * @tparam ReceiveBuffer The buffer type to use for ServerConnection instances
 */
template <typename ReceiveBuffer = ArrayBufferAdapter<4096>>
class TcpServer
{
public:
    using ConnectionPtr      = std::shared_ptr<ServerConnection<ReceiveBuffer>>;
    using ConnectionCallback = std::function<void(ConnectionPtr)>;

    /**
     * @brief Construct a TCP server
     * 
     * @param io_context The Boost.Asio io_context to use
     * @param port The port to listen on
     * @param keepalive_interval Keepalive interval for accepted connections
     */
    TcpServer(boost::asio::io_context& io_context, unsigned short port,
              std::chrono::seconds keepalive_interval = ServerConnection<ReceiveBuffer>::default_keepalive_interval);

    /**
     * @brief Construct a TCP server with specific endpoint
     * 
     * @param io_context The Boost.Asio io_context to use
     * @param endpoint The endpoint to bind to (address + port)
     * @param keepalive_interval Keepalive interval for accepted connections
     */
    TcpServer(boost::asio::io_context& io_context, const boost::asio::ip::tcp::endpoint& endpoint,
              std::chrono::seconds keepalive_interval = ServerConnection<ReceiveBuffer>::default_keepalive_interval);

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
     * @param callback Function to call with each new ServerConnection
     */
    void set_connection_callback(ConnectionCallback callback) { _connection_callback = std::move(callback); }

    /**
     * @brief Start accepting connections
     * 
     * Begins listening for and accepting new connections.
     */
    void start();

    /**
     * @brief Stop accepting connections
     * 
     * Stops the acceptor and closes any pending accept operations.
     */
    void stop();

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
    void start_accept();
    void handle_accept(const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket);

    boost::asio::io_context& _io_context;
    boost::asio::ip::tcp::acceptor _acceptor;
    std::chrono::seconds _keepalive_interval;
    ConnectionCallback _connection_callback;
    bool _is_running;
};

// Template implementation

template <typename ReceiveBuffer>
TcpServer<ReceiveBuffer>::TcpServer(boost::asio::io_context& io_context, unsigned short port, std::chrono::seconds keepalive_interval)
    : _io_context(io_context)
    , _acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    , _keepalive_interval(keepalive_interval)
    , _is_running(false)
{
}

template <typename ReceiveBuffer>
TcpServer<ReceiveBuffer>::TcpServer(boost::asio::io_context& io_context, const boost::asio::ip::tcp::endpoint& endpoint,
                                    std::chrono::seconds keepalive_interval)
    : _io_context(io_context), _acceptor(io_context, endpoint), _keepalive_interval(keepalive_interval), _is_running(false)
{
}

template <typename ReceiveBuffer>
void TcpServer<ReceiveBuffer>::start()
{
    if (_is_running)
    {
        return;
    }

    _is_running = true;
    start_accept();
}

template <typename ReceiveBuffer>
void TcpServer<ReceiveBuffer>::stop()
{
    if (!_is_running)
    {
        return;
    }

    _is_running = false;

    boost::system::error_code error_code;
    _acceptor.close(error_code);
    // Ignore errors on close
}

template <typename ReceiveBuffer>
void TcpServer<ReceiveBuffer>::start_accept()
{
    if (!_is_running)
    {
        return;
    }

    _acceptor.async_accept([this](const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket)
                           { handle_accept(error_code, std::move(socket)); });
}

template <typename ReceiveBuffer>
void TcpServer<ReceiveBuffer>::handle_accept(const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket)
{
    if (!error_code)
    {
        // Create a ServerConnection for the accepted socket
        auto connection = std::make_shared<ServerConnection<ReceiveBuffer>>(std::move(socket), _io_context, _keepalive_interval);

        // Start monitoring the connection
        connection->start_monitoring();

        // Notify the user
        if (_connection_callback)
        {
            _connection_callback(connection);
        }
    }
    // If there was an error, we just continue accepting unless stopped

    // Accept the next connection
    if (_is_running)
    {
        start_accept();
    }
}

} // namespace eestv
