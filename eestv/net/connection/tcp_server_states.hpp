#pragma once

namespace eestv
{

enum class TcpServerState
{
    start_signaled,
    stop_signaled,
    starting_accept,
    accepting,
    stopping

};

inline const char* to_string(TcpServerState state) noexcept
{
    switch (state)
    {
    case TcpServerState::start_signaled:
        return "start_signaled";
    case TcpServerState::stop_signaled:
        return "stop_signaled";
    case TcpServerState::starting_accept:
        return "starting_accept";
    case TcpServerState::accepting:
        return "accepting";
    case TcpServerState::stopping:
        return "stopping";
    }
    return "unknown";
}
} // namespace eestv
