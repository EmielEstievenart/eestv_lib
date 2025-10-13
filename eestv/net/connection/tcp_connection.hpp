#pragma once

#include "boost/asio/error.hpp"
#include "eestv/data/linear_buffer.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "eestv/logging/eestv_logging.hpp"
#include "eestv/flags/flags.hpp"
#include "tcp_connection_states.hpp"
#include <boost/asio.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace eestv
{

template <typename ReceiveBuffer = LinearBuffer, typename SendBuffer = LinearBuffer>
class TcpConnection
{
public:
    using OnConnectionLostCallback = std::function<void()>;
    using OnDataReceivedCallback   = std::function<void()>;

    static constexpr std::size_t receive_buffer_size = 4096;

    TcpConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context, std::size_t receive_buffer_size,
                  std::size_t send_buffer_size);

    TcpConnection(const TcpConnection&)            = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&&)                 = delete;
    TcpConnection& operator=(TcpConnection&&)      = delete;

    ~TcpConnection();

    void set_connection_lost_callback(OnConnectionLostCallback callback) { _connection_lost_callback = std::move(callback); }
    void set_data_received_callback(OnDataReceivedCallback callback) { _data_received_callback = std::move(callback); }

    // Access to buffers for user code
    ReceiveBuffer& receive_buffer() { return _receive_buffer; }
    const ReceiveBuffer& receive_buffer() const { return _receive_buffer; }
    SendBuffer& send_buffer() { return _send_buffer; }
    const SendBuffer& send_buffer() const { return _send_buffer; }

    /*This is to be called everytime the user has prepared data to be send via the buffers. */
    void start_sending();

    void start_receiving();

    void asycn_disconnect();

private:
    mutable std::mutex _mutex;

    void set_flag_thread_safe(TcpConnectionState state)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _flags.set_flag(state);
    }

    void clear_flag_thread_safe(TcpConnectionState state)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _flags.clear_flag(state);
    }

    bool get_flag_thread_safe(TcpConnectionState state) const
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _flags.get_flag(state);
    }

    void async_receive();
    void async_send();
    // Synchronous (immediate) disconnect callable from io_context thread
    void disconnect();

    boost::asio::io_context& _io_context;
    boost::asio::ip::tcp::socket _socket;
    boost::asio::ip::tcp::endpoint _remote_endpoint;

    Flags<TcpConnectionState> _flags;

    void on_receive(const boost::system::error_code& error, std::size_t bytes_transferred);
    void on_send(const boost::system::error_code& error, std::size_t bytes_transferred);

    std::chrono::steady_clock::time_point _last_receive_timepoint {std::chrono::steady_clock::now()};
    ReceiveBuffer _receive_buffer;
    SendBuffer _send_buffer;
    OnConnectionLostCallback _connection_lost_callback;
    OnDataReceivedCallback _data_received_callback;
};

// Template implementation

template <typename ReceiveBuffer, typename SendBuffer>
TcpConnection<ReceiveBuffer, SendBuffer>::TcpConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context,
                                                        std::size_t receive_buffer_size, std::size_t send_buffer_size)
    : _io_context {io_context}, _socket(std::move(socket)), _receive_buffer(receive_buffer_size), _send_buffer(send_buffer_size)
{
    try
    {
        _remote_endpoint = _socket.remote_endpoint();
        // EESTV_LOG_DEBUG("TcpConnection established with " << _remote_endpoint.address().to_string() << ":" << _remote_endpoint.port());
    }
    catch (const boost::system::system_error& e)
    {
        // EESTV_LOG_ERROR("Could not get remote endpoint: " << e.what());
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
TcpConnection<ReceiveBuffer, SendBuffer>::~TcpConnection()
{
    asycn_disconnect();

    while (get_flag_thread_safe(TcpConnectionState::sending) || get_flag_thread_safe(TcpConnectionState::receiving) ||
           get_flag_thread_safe(TcpConnectionState::closing))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::asycn_disconnect()
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (!_flags.get_flag(TcpConnectionState::closing))
    {
        _flags.set_flag(TcpConnectionState::closing);
        boost::asio::post(_io_context, [this]() { this->disconnect(); });
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::disconnect()
{
    boost::system::error_code error_code;
    if (_socket.is_open())
    {
        _socket.close(error_code);
        if (error_code)
        {
            EESTV_LOG_DEBUG("Error closing socket: " << error_code.message());
        }
    }
    clear_flag_thread_safe(TcpConnectionState::closing);
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::start_receiving()
{
    {
        std::unique_lock<std::mutex> lock(_mutex);

        if (_flags.get_flag(TcpConnectionState::receiving) || _flags.get_flag(TcpConnectionState::closing))
        {
            return;
        }
        _flags.set_flag(TcpConnectionState::receiving);
        boost::asio::post(_io_context, [this]() { this->async_receive(); });
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::async_receive()
{
    // Get write head for receiving data
    std::size_t writable_size = 0;
    std::uint8_t* write_head  = _receive_buffer.get_write_head(writable_size);

    if (write_head == nullptr || writable_size == 0)
    {
        EESTV_LOG_ERROR("Receive buffer full or not available, cannot receive more data.");
        return;
    }

    // Safe to use raw 'this' because destructor waits for all operations to complete
    _socket.async_read_some(boost::asio::buffer(write_head, writable_size),
                            [this](const boost::system::error_code& error_code, std::size_t bytes_transferred)
                            { this->on_receive(error_code, bytes_transferred); });
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::on_receive(const boost::system::error_code& error_code, std::size_t bytes_transferred)
{
    if (error_code)
    {
        //print the error code

        if (error_code == boost::asio::error::operation_aborted)
        {
            EESTV_LOG_INFO("Receive operation aborted on endpoint " << _remote_endpoint.address().to_string() << ":"
                                                                    << _remote_endpoint.port());
        }
        else
        {
            EESTV_LOG_ERROR("Receive operation failed " << error_code.message() << " (code=" << error_code.value()
                                                        << ", category=" << error_code.category().name() << ") "
                                                        << "on endpoint " << _remote_endpoint.address().to_string() << " : "
                                                        << _remote_endpoint.port());
        }
        _flags.clear_flag(TcpConnectionState::receiving);
        if (_connection_lost_callback)
        {
            _connection_lost_callback();
        }
        return;
    }

    _receive_buffer.commit(bytes_transferred);
    _last_receive_timepoint = std::chrono::steady_clock::now();
    if (_data_received_callback != nullptr)
    {
        _data_received_callback();
    }
    async_receive();
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::start_sending()
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (!_flags.get_flag(TcpConnectionState::sending) && !_flags.get_flag(TcpConnectionState::closing))
    {
        _flags.set_flag(TcpConnectionState::sending);
        boost::asio::post(_io_context,
                          [this]()
                          {
                              // Schedule the send on the io_context to ensure async_write is
                              // initiated from the I/O thread.
                              this->async_send();
                          });
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::async_send()
{
    std::size_t bytes_to_send {0};
    const std::uint8_t* send_ptr = _send_buffer.get_read_head(bytes_to_send);

    if (bytes_to_send == 0 || send_ptr == nullptr)
    {
        clear_flag_thread_safe(TcpConnectionState::sending);
        return;
    }

    // Safe to use raw 'this' because destructor waits for all operations to complete
    boost::asio::async_write(_socket, boost::asio::buffer(send_ptr, bytes_to_send),
                             [this](const boost::system::error_code& error_code, std::size_t bytes_transferred)
                             { this->on_send(error_code, bytes_transferred); });
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::on_send(const boost::system::error_code& error_code, std::size_t bytes_transferred)
{
    if (error_code)
    {
        if (error_code == boost::asio::error::operation_aborted)
        {
            EESTV_LOG_INFO("Send operation aborted " << _remote_endpoint.address().to_string() << ":" << _remote_endpoint.port());
        }
        else
        {
            EESTV_LOG_ERROR("Send operation failed  " << error_code.message() << " (code=" << error_code.value()
                                                      << ", category=" << error_code.category().name() << ") "
                                                      << "on endpoint " << _remote_endpoint.address().to_string() << " : "
                                                      << _remote_endpoint.port());
        }
        clear_flag_thread_safe(TcpConnectionState::sending);
        if (_connection_lost_callback)
        {
            _connection_lost_callback();
        }
        return;
    }

    _send_buffer.consume(bytes_transferred);

    std::size_t remaining_size {0};
    if (_send_buffer.get_read_head(remaining_size) != nullptr && remaining_size > 0)
    {
        async_send();
    }
    else
    {
        clear_flag_thread_safe(TcpConnectionState::sending);
    }
}

} // namespace eestv
