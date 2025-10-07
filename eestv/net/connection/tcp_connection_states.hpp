#pragma once

namespace eestv
{
enum class TcpConnectionState
{
    connected,
    monitoring,
    lost,
    dead
};

inline const char* to_string(TcpConnectionState state) noexcept
{
    switch (state)
    {
    case TcpConnectionState::connected:
        return "connected";
    case TcpConnectionState::monitoring:
        return "monitoring";
    case TcpConnectionState::lost:
        return "lost";
    case TcpConnectionState::dead:
        return "dead";
    }
    return "unknown";
}

} // namespace eestv