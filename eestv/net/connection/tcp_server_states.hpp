#pragma once

namespace eestv
{

enum class TcpServerState
{
    accepting,
    closing
};

inline const char* to_string(TcpServerState state) noexcept
{
    switch (state)
    {
    case TcpServerState::accepting:
        return "accepting";
    case TcpServerState::closing:
        return "closing";
    }
    return "unknown";
}
} // namespace eestv
