#pragma once

namespace eestv
{
enum class UdpDiscoveryState
{
    running,
    stopping,
    timer_running,
    sending_async,
    receiving_async,
};

inline const char* to_string(UdpDiscoveryState state) noexcept
{
    switch (state)
    {
    case UdpDiscoveryState::running:
        return "running";

    case UdpDiscoveryState::stopping:
        return "stopping";

    case UdpDiscoveryState::timer_running:
        return "timer_running";

    case UdpDiscoveryState::sending_async:
        return "sending_async";

    case UdpDiscoveryState::receiving_async:
        return "receiving_async";
    }

    return "unknown";
}

} // namespace eestv