#pragma once

namespace eestv
{
class DataBridge
{
public:
    DataBridge(int argc, char* argv[]);

private:
    void parse_command_line_parameters(int argc, char* argv[]);
};
}
