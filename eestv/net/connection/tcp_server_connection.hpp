#pragma once

#include "eestv/net/connection/tcp_connection.hpp"

namespace eestv
{

template <typename ReceiveBuffer = ArrayBufferAdapter<4096>>
class TcpServerConnection : public TcpConnection<ReceiveBuffer>
{
public:
    TcpServerConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context,
                        std::chrono::seconds keepalive_interval = TcpConnection<ReceiveBuffer>::default_keepalive_interval);

    ~TcpServerConnection() override = default;

protected:
    void on_connection_lost() override;
};

// Template implementation

template <typename ReceiveBuffer>
TcpServerConnection<ReceiveBuffer>::TcpServerConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context,
                                                        std::chrono::seconds keepalive_interval)
    : TcpConnection<ReceiveBuffer>(std::move(socket), io_context, keepalive_interval)
{
}

template <typename ReceiveBuffer>
void TcpServerConnection<ReceiveBuffer>::on_connection_lost()
{
    // EESTV_LOG_INFO("Server connection lost, marking as dead");
    this->set_state(typename TcpConnection<ReceiveBuffer>::State::dead);
}

} // namespace eestv
