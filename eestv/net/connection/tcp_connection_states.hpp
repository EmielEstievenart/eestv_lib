#pragma once

namespace eestv
{
enum class TcpConnectionState
{
    start_sending,
    sending,
    start_receiving,
    receiving,
    closing,
    stop_signaled,
    lost
};

inline const char* to_string(TcpConnectionState state) noexcept
{
    switch (state)
    {
    case TcpConnectionState::start_sending:
        return "start_sending";
    case TcpConnectionState::sending:
        return "sending";
    case TcpConnectionState::start_receiving:
        return "start_receiving";
    case TcpConnectionState::receiving:
        return "receiving";
    case TcpConnectionState::closing:
        return "closing";
    case TcpConnectionState::stop_signaled:
        return "stop_signaled";
    case TcpConnectionState::lost:
        return "lost";
    }
    return "unknown";
}

} // namespace eestv