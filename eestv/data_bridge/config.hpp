#pragma once

#include <string>

#include "eestv/logging/eestv_logging.hpp"

namespace eestv::bridge
{

enum class EndpointMode
{
    endpoint,
    bridge
};

struct Config
{
    EndpointMode endpoint_mode;
    std::string discovery_target;
    logging::LogLevel log_level = logging::LogLevel::Info;
};

} // namespace eestv::bridge