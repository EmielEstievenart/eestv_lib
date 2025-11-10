#include "eestv/net/discovery/discoverable.hpp"

namespace eestv
{
Discoverable::Discoverable(const std::string& identifier, std::function<std::string(const boost::asio::ip::udp::endpoint&)> callback)
    : _identifier(identifier), _callback(callback)
{
}

const std::string& Discoverable::get_identifier() const
{
    return _identifier;
}

std::string Discoverable::get_reply(const boost::asio::ip::udp::endpoint& remote_endpoint) const
{
    return _callback(remote_endpoint);
}

} // namespace eestv
