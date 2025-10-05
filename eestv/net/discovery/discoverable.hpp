#pragma once

#include <string>
#include <functional>

class Discoverable
{
public:
    Discoverable(const std::string& identifier, std::function<std::string()> callback);

    const std::string& get_identifier() const;
    std::string get_reply() const;

private:
    std::string _identifier;
    std::function<std::string()> _callback;
};
