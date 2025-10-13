#pragma once

namespace eestv
{
enum class TcpConnectionState
{
    sending,
    receiving,
    closing,
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
    case TcpConnectionState::sending:
        return "sending";
    case TcpConnectionState::receiving:
        return "receiving";
    case TcpConnectionState::closing:
        return "closing";
        break;
    }
    return "unknown";
}

} // namespace eestv