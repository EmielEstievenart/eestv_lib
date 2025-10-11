#pragma once

#include "eestv/net/connection/tcp_connection.hpp"

namespace eestv
{

template <typename ReceiveBuffer = LinearBuffer, typename SendBuffer = LinearBuffer>
class TcpServerConnection : public TcpConnection<ReceiveBuffer, SendBuffer>
{
public:
    static constexpr std::size_t default_buffer_size = 4096;

    TcpServerConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context,
                        std::size_t receive_buffer_size = default_buffer_size, std::size_t send_buffer_size = default_buffer_size);

    ~TcpServerConnection() override = default;

protected:
    void on_connection_lost() override;
};

// Template implementation

template <typename ReceiveBuffer, typename SendBuffer>
TcpServerConnection<ReceiveBuffer, SendBuffer>::TcpServerConnection(boost::asio::ip::tcp::socket&& socket,
                                                                    boost::asio::io_context& io_context, std::size_t receive_buffer_size,
                                                                    std::size_t send_buffer_size)
    : TcpConnection<ReceiveBuffer, SendBuffer>(std::move(socket), io_context, receive_buffer_size, send_buffer_size)
{
}

template <typename ReceiveBuffer, typename SendBuffer>
void TcpServerConnection<ReceiveBuffer, SendBuffer>::on_connection_lost()
{
    // EESTV_LOG_INFO("Server connection lost");
}

} // namespace eestv
