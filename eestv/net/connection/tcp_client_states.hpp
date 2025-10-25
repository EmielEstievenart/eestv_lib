#pragma once

#include <cstdint>

namespace eestv
{
enum class TcpClientState : std::uint8_t
{
    schedule_resolving,
    resolving,
    connecting,
    stopping,
    stop_singaled
};

inline const char* to_string(TcpClientState state) noexcept
{
    switch (state)
    {
    case TcpClientState::schedule_resolving:
        return "schedule_resolving";
    case TcpClientState::resolving:
        return "resolving";
    case TcpClientState::connecting:
        return "connecting";
    case TcpClientState::stopping:
        return "stopping";
    case TcpClientState::stop_singaled:
        return "stop_singaled";
    }
    return "unknown";
}

} // namespace eestv
