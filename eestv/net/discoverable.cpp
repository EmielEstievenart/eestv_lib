#include "eestv/net/discoverable.hpp"

Discoverable::Discoverable(const std::string& identifier, std::function<std::string()> callback)
    : _identifier(identifier), _callback(callback)
{
}

const std::string& Discoverable::get_identifier() const
{
    return _identifier;
}

std::string Discoverable::get_reply() const
{
    return _callback();
}
