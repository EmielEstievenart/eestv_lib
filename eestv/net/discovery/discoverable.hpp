#pragma once

#include <string>
#include <functional>

/**
 * @brief Represent a service that can be discovered on the network.
 *
 * Holds a short identifier and a callback that produces the discovery reply.
 * Example: Discoverable("svc", []{ return std::string("v=1"); });
 */
class Discoverable
{
public:
    /**
     * @brief Create a discoverable service.
     * @param identifier Short unique identifier.
     * @param callback Callable returning a reply string.
     */
    Discoverable(const std::string& identifier, std::function<std::string()> callback);

    /// Return the identifier given at construction.
    const std::string& get_identifier() const;

    /// Call the callback and return its reply string.
    std::string get_reply() const;

private:
    std::string _identifier;
    std::function<std::string()> _callback;
};
