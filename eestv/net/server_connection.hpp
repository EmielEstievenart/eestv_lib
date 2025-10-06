#pragma once

#include "eestv/net/connection.hpp"

namespace eestv
{

template <typename ReceiveBuffer = ArrayBufferAdapter<4096>>
class ServerConnection : public Connection<ReceiveBuffer>
{
public:
    ServerConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context,
                     std::chrono::seconds keepalive_interval = Connection<ReceiveBuffer>::default_keepalive_interval);

    ~ServerConnection() override = default;

protected:
    void on_connection_lost() override;
};

// Template implementation

template <typename ReceiveBuffer>
ServerConnection<ReceiveBuffer>::ServerConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::io_context& io_context,
                                                  std::chrono::seconds keepalive_interval)
    : Connection<ReceiveBuffer>(std::move(socket), io_context, keepalive_interval)
{
}

template <typename ReceiveBuffer>
void ServerConnection<ReceiveBuffer>::on_connection_lost()
{
    // EESTV_LOG_INFO("Server connection lost, marking as dead");
    this->set_state(typename Connection<ReceiveBuffer>::State::dead);
}

} // namespace eestv
