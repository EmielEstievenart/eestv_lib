#pragma once

#include "eestv/net/connection/tcp_connection.hpp"
#include "eestv/logging/eestv_logging.hpp"
#include "eestv/flags/flags.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace eestv
{

enum class TcpServerState
{
    accepting,
    closing
};

inline const char* to_string(TcpServerState state) noexcept
{
    switch (state)
    {
    case TcpServerState::accepting:
        return "accepting";
    case TcpServerState::closing:
        return "closing";
    }
    return "unknown";
}

/**
 * @brief TCP server that accepts connections and creates TcpConnection instances
 * 
 * The TcpServer listens on a specified port and automatically accepts incoming
 * connections. Each accepted connection is wrapped in a TcpConnection and
 * passed to a user-provided callback.
 * 
 * @tparam ReceiveBuffer The buffer type to use for receiving data in TcpConnection instances
 * @tparam SendBuffer The buffer type to use for sending data in TcpConnection instances
 */
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

    /**
     * @brief Set the callback for new connections
     * 
     * This callback is invoked whenever a new client connects.
     * 
     * @param callback Function to call with each new TcpConnection
     */
    void set_connection_callback(ConnectionCallback callback)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _connection_callback = std::move(callback);
    }

    /**
     * @brief Set the callback for when the server stops
     * 
     * This callback is invoked when the server has fully stopped accepting connections.
     * 
     * @param callback Function to call when the server stops
     */
    void set_stopped_callback(StoppedCallback callback)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _stopped_callback = std::move(callback);
    }

    /**
     * @brief Start accepting connections (posts to io_context)
     * 
     * Begins listening for and accepting new connections.
     * This operation is posted to the io_context to avoid race conditions.
     */
    void async_start();

    /**
     * @brief Stop accepting connections (posts to io_context)
     * 
     * Stops the acceptor and closes any pending accept operations.
     * This operation is posted to the io_context to avoid race conditions.
     */
    void async_stop();

    /**
     * @brief Check if the server is currently accepting connections
     * 
     * @return true if accepting, false otherwise
     */
    bool is_running() const { return get_flag_thread_safe(TcpServerState::accepting); }

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
    void stop();
    void async_accept();
    void handle_accept(const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket);

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
    async_stop();

    while (get_flag_thread_safe(TcpServerState::accepting) || get_flag_thread_safe(TcpServerState::closing))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::async_start()
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (!_flags.get_flag(TcpServerState::accepting))
    {
        _flags.set_flag(TcpServerState::accepting);
        boost::asio::post(_io_context, [this]() { this->async_accept(); });
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::async_stop()
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (!_flags.get_flag(TcpServerState::closing) && _flags.get_flag(TcpServerState::accepting))
    {
        _flags.set_flag(TcpServerState::closing);
        boost::asio::post(_io_context, [this]() { this->stop(); });
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::stop()
{
    std::unique_lock<std::mutex> lock(_mutex);
    boost::system::error_code error_code;
    // Even though the docs say it will cancel immediately, that is not true. It cancels immediately **on the io_context's thread**
    _acceptor.close(error_code);
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServer<ReceiveBuffer, SendBuffer>::async_accept()
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
        _flags.clear_flag(TcpServerState::closing);

        if (_stopped_callback)
        {
            EESTV_LOG_INFO("The server has stopped. ");
            _stopped_callback();
        }

        return;
    }

    auto connection =
        std::make_unique<TcpConnection<ReceiveBuffer, SendBuffer>>(std::move(socket), _io_context, _receive_buffer_size, _send_buffer_size);
    if (_connection_callback)
    {
        _connection_callback(std::move(connection));
    }

    async_accept();
}

} // namespace eestv
