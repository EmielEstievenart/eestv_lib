#pragma once

#include <string>

#include "eestv/logging/eestv_logging.hpp"

namespace eestv
{

enum class EndpointMode
{
    endpoint,
    bridge
};

struct DataBridgeConfig
{
    EndpointMode endpoint_mode;
    std::string discovery_target;
    logging::LogLevel log_level = logging::LogLevel::Info;
};

} // namespace eestv