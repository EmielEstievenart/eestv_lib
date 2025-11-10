#pragma once

#include <string>
#include <functional>
#include "boost/asio/ip/udp.hpp"

namespace eestv
{

/**
 * @brief Represent a service that can be discovered on the network.
 *
 * Holds a short identifier and a callback that produces the discovery reply.
 * The callback receives the remote endpoint that initiated the discovery request,
 * allowing services to determine the appropriate IP address to reply with when
 * listening on multiple network interfaces.
 *
 * Example: Discoverable("svc", [](const auto& endpoint){ return std::string("v=1"); });
 */
class Discoverable
{
public:
    /**
     * @brief Create a discoverable service.
     * @param identifier Short unique identifier.
     * @param callback Callable that receives the remote endpoint and returns a reply string.
     */
    Discoverable(const std::string& identifier, std::function<std::string(const boost::asio::ip::udp::endpoint&)> callback);

    /// Return the identifier given at construction.
    const std::string& get_identifier() const;

    /// Call the callback with the remote endpoint and return its reply string.
    std::string get_reply(const boost::asio::ip::udp::endpoint& remote_endpoint) const;

private:
    std::string _identifier;
    std::function<std::string(const boost::asio::ip::udp::endpoint&)> _callback;
};
} // namespace eestv