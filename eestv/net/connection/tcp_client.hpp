#pragma once

#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "eestv/flags/flags.hpp"
#include "eestv/net/connection/tcp_client_states.hpp"

#include <functional>
#include <mutex>
#include <string>

namespace eestv
{

class TcpClient
{
public:
    using ConnectionCallback = std::function<void(boost::asio::ip::tcp::socket&&, const boost::system::error_code&)>;

    TcpClient(boost::asio::io_context& io_context);

    ~TcpClient();

    TcpClient(const TcpClient&)            = delete;
    TcpClient& operator=(const TcpClient&) = delete;
    TcpClient(TcpClient&&)                 = delete;
    TcpClient& operator=(TcpClient&&)      = delete;

    /**
     * Asynchronously connect to a server
     * @param host The hostname or IP address of the server
     * @param port The port number
     * @param on_connection_complete Callback invoked with the socket and error code
     *                               If error_code is empty, connection succeeded and socket is valid
     *                               If error_code is set, connection failed and socket is invalid
     * @return true if connection attempt started, false if already connecting
     */
    bool async_connect(const std::string& host, int port, ConnectionCallback on_connection_complete);

    /**
     * Stop any ongoing connection attempt
     * @param on_stopped Callback invoked when stop is complete
     * @return true if stop initiated, false if not connecting
     */
    bool async_stop(std::function<void()> on_stopped);

    /**
     * Synchronously stop any ongoing connection attempt
     */
    void stop();

    /**
     * Reset the client state after a successful stop
     * Must be called after a successful stop if you want to connect again
     */
    void reset();

    /**
     * Get a string representation of the current state flags
     * @return String showing which state flags are set
     */
    std::string to_string() const;

private:
    void async_resolve(const std::string& host, int port);
    void handle_resolve(const boost::system::error_code& error_code, boost::asio::ip::tcp::resolver::results_type results);

    void handle_connect(const boost::system::error_code& error_code);

    void resolve_on_stopped();

    mutable std::mutex _mutex;
    boost::asio::io_context& _io_context;
    boost::asio::ip::tcp::socket _socket;
    boost::asio::ip::tcp::resolver _resolver;

    Flags<TcpClientState> _flags;

    ConnectionCallback _on_connection_complete;
    std::function<void()> _on_stopped;
};

} // namespace eestv
