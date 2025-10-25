#pragma once

#include "boost/asio/error.hpp"
#include "boost/asio/post.hpp"
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
#include <sstream>
#include <thread>
#include <utility>

namespace eestv
{

template <typename ReceiveBuffer = LinearBuffer, typename SendBuffer = LinearBuffer>
class TcpConnection
{
public:
    using OnConnectionLostCallback = std::function<void()>;
    using OnDataReceivedCallback   = std::function<void()>;
    using StoppedCallback          = std::function<void()>;

    static constexpr std::size_t receive_buffer_size = 4096;

    TcpConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context, std::size_t receive_buffer_size,
                  std::size_t send_buffer_size);

    TcpConnection(const TcpConnection&)            = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&&)                 = delete;
    TcpConnection& operator=(TcpConnection&&)      = delete;

    ~TcpConnection();

    void set_connection_lost_callback(OnConnectionLostCallback callback) { _connection_lost_callback = std::move(callback); }
    /*  Warning, the _data_received_callback will be called while holding a lock. 
        DO NOT address the tcp connection whatsoever. 
        If you must initiate something, post to the io_context. */
    void set_data_received_callback(OnDataReceivedCallback callback) { _data_received_callback = std::move(callback); }

    // Access to buffers for user code
    ReceiveBuffer& receive_buffer() { return _receive_buffer; }
    const ReceiveBuffer& receive_buffer() const { return _receive_buffer; }
    SendBuffer& send_buffer() { return _send_buffer; }
    const SendBuffer& send_buffer() const { return _send_buffer; }

    /*This is to be called everytime the user has prepared data to be send via the buffers. */
    void start_sending();

    void start_receiving();

    /*
    Stop doesn't close the socket, but it cancels the send and receive operations. This is the pre-cursor to closing the socket via the destruction of this object. 
    */
    bool asycn_stop(StoppedCallback on_disconnected);

    void stop();

private:
    mutable std::mutex _mutex;

    void cancel_async_operations();

    void async_receive();
    void async_send();
    // Synchronous (immediate) disconnect callable from io_context thread

    boost::asio::io_context& _io_context;
    boost::asio::ip::tcp::socket _socket;
    boost::asio::ip::tcp::endpoint _remote_endpoint;

    Flags<TcpConnectionState> _flags;

    void on_receive(const boost::system::error_code& error, std::size_t bytes_transferred);
    void on_send(const boost::system::error_code& error, std::size_t bytes_transferred);

    ReceiveBuffer _receive_buffer;
    SendBuffer _send_buffer;
    OnConnectionLostCallback _connection_lost_callback;
    OnDataReceivedCallback _data_received_callback;
    StoppedCallback _disconnected_callback;

    void resolve_on_stopped();
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
    stop();
}

template <typename ReceiveBuffer, typename SendBuffer>
bool TcpConnection<ReceiveBuffer, SendBuffer>::asycn_stop(StoppedCallback on_disconnected)
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (!_flags.get_flag(TcpConnectionState::stop_signaled))
    {
        _flags.set_flag(TcpConnectionState::stop_signaled);
        _flags.set_flag(TcpConnectionState::closing);
        _disconnected_callback = std::move(on_disconnected);
        boost::asio::post(_io_context, [this]() { cancel_async_operations(); });
        return true;
    }
    return false;
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::stop()
{
    std::atomic_bool stopped = false;
    if (asycn_stop([&stopped]() { stopped = true; }))
    {
        while (!stopped)
        {
        }
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::resolve_on_stopped()
{
    if (!(_flags.get_flag(TcpConnectionState::closing) || _flags.get_flag(TcpConnectionState::start_sending) ||
          _flags.get_flag(TcpConnectionState::sending) || _flags.get_flag(TcpConnectionState::start_receiving) ||
          _flags.get_flag(TcpConnectionState::receiving)))
    {
        auto disconnected_callback = std::move(_disconnected_callback);
        _disconnected_callback     = nullptr;

        if (disconnected_callback)
        {
            boost::asio::post(_io_context,
                              [disconnected_callback = std::move(disconnected_callback)]() mutable
                              {
                                  if (disconnected_callback)
                                  {
                                      disconnected_callback();
                                  }
                              });
        }
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::cancel_async_operations()
{
    boost::system::error_code error_code;
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _socket.cancel(error_code);
        _flags.clear_flag(TcpConnectionState::closing);
        resolve_on_stopped();
    }
    if (error_code)
    {
        EESTV_LOG_ERROR("Error cancelling async operations on socket: " << error_code.message());
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::start_receiving()
{
    std::unique_lock<std::mutex> lock(_mutex);

    if (!(_flags.get_flag(TcpConnectionState::start_receiving) || _flags.get_flag(TcpConnectionState::receiving) ||
          _flags.get_flag(TcpConnectionState::stop_signaled)))
    {
        _flags.set_flag(TcpConnectionState::start_receiving);
        boost::asio::post(_io_context,
                          [this]()
                          {
                              std::unique_lock<std::mutex> lock(_mutex);
                              this->async_receive();
                          });
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::async_receive()
{
    _flags.clear_flag(TcpConnectionState::start_receiving);
    if (!_flags.get_flag(TcpConnectionState::stop_signaled))
    {
        std::size_t writable_size = 0;
        std::uint8_t* write_head  = _receive_buffer.get_write_head(writable_size);

        if (write_head == nullptr || writable_size == 0)
        {
            EESTV_LOG_ERROR("Receive buffer full or not available, cannot receive more data.");
            return;
        }

        _socket.async_read_some(boost::asio::buffer(write_head, writable_size),
                                [this](const boost::system::error_code& error_code, std::size_t bytes_transferred)
                                { this->on_receive(error_code, bytes_transferred); });
    }
    else
    {
        resolve_on_stopped();
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::on_receive(const boost::system::error_code& error_code, std::size_t bytes_transferred)
{
    std::unique_lock<std::mutex> lock(_mutex, std::defer_lock);
    if (error_code)
    {
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
        lock.lock();
        _flags.clear_flag(TcpConnectionState::receiving);
        if (_flags.get_flag(TcpConnectionState::stop_signaled))
        {
            resolve_on_stopped();
        }
        if (_connection_lost_callback)
        {
            boost::asio::post(_io_context, [this]() { this->_connection_lost_callback(); });
        }
    }
    else
    {
        lock.lock();
        _receive_buffer.commit(bytes_transferred);
        if (_data_received_callback != nullptr)
        {
            // This function is called immediately because its important for the data structure to be behind a lock and usable.
            // We must provide the user the possibility to consume all bytes and reset the linear datastructure before again reserving a  region.
            _data_received_callback();
        }
        async_receive();
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::start_sending()
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (!(_flags.get_flag(TcpConnectionState::start_sending) || _flags.get_flag(TcpConnectionState::sending) ||
          _flags.get_flag(TcpConnectionState::stop_signaled)))
    {
        _flags.set_flag(TcpConnectionState::start_sending);

        boost::asio::post(_io_context,
                          [this]()
                          {
                              std::unique_lock<std::mutex> lock(_mutex);
                              _flags.clear_flag(TcpConnectionState::start_sending);
                              if (!_flags.get_flag(TcpConnectionState::stop_signaled))
                              {
                                  _flags.set_flag(TcpConnectionState::sending);
                                  this->async_send();
                              }
                              else
                              {
                                  resolve_on_stopped();
                              }
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
        _flags.clear_flag(TcpConnectionState::sending);
    }
    else
    {
        boost::asio::async_write(_socket, boost::asio::buffer(send_ptr, bytes_to_send),
                                 [this](const boost::system::error_code& error_code, std::size_t bytes_transferred)
                                 { this->on_send(error_code, bytes_transferred); });
    }
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpConnection<ReceiveBuffer, SendBuffer>::on_send(const boost::system::error_code& error_code, std::size_t bytes_transferred)
{
    std::unique_lock<std::mutex> lock(_mutex, std::defer_lock);
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

        lock.lock();

        _flags.clear_flag(TcpConnectionState::sending);
        if (_flags.get_flag(TcpConnectionState::stop_signaled))
        {
            resolve_on_stopped();
        }
        if (_connection_lost_callback)
        {
            boost::asio::post(_io_context, [this]() { this->_connection_lost_callback(); });
        }
    }
    else
    {
        lock.lock();
        _send_buffer.consume(bytes_transferred);
        /*async_send clears the flag if nothing is left to be send*/
        async_send();
        if (!_flags.get_flag(TcpConnectionState::sending))
        {
            if (_flags.get_flag(TcpConnectionState::stop_signaled))
            {
                resolve_on_stopped();
            }
        }
    }
}

} // namespace eestv
