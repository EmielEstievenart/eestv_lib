#include "data_bridge_cli.hpp"
#include "eestv/logging/eestv_logging.hpp"

#include <boost/program_options.hpp>
#include <iostream>
#include <vector>

namespace eestv
{

DataBridgeConfig parse_command_line(int argc, char* argv[])
{
    namespace po = boost::program_options;

    po::options_description desc("DataBridge Options");
    // clang-format off
    desc.add_options()
        ("help,h", "Show help message")
        ("endpoint,e", "Run as endpoint")
        ("bridge,b", "Run as bridge")
        ("discovery,d", po::value<std::string>()->required(), "Discovery identifier (required)");
    // clang-format on

    std::vector<std::string> arguments;
    arguments.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i)
    {
        arguments.emplace_back(argv[i]);
    }

    int verbosity = 0;
    for (size_t i = 1; i < arguments.size(); ++i)
    {
        const std::string& argument = arguments[i];
        if (argument.size() >= 2 && argument[0] == '-' && argument[1] == 'v')
        {
            size_t v_count = 0;
            for (size_t j = 1; j < argument.size() && argument[j] == 'v'; ++j)
            {
                ++v_count;
            }

            if (v_count == argument.size() - 1)
            {
                verbosity = static_cast<int>(v_count);
                break;
            }
        }
    }

    po::variables_map variables;

    DataBridgeConfig config;

    try
    {
        auto parser = po::command_line_parser(arguments).options(desc).allow_unregistered();

        po::store(parser.run(), variables);
        po::notify(variables);

        if (variables.count("help") != 0U)
        {
            std::cout << desc << '\n';
            std::cout << "\nVerbosity levels:\n"
                      << "  (none)  : Info level  - shows ERROR and INFO messages\n"
                      << "  -v      : Debug level - shows ERROR, INFO, and DEBUG messages\n"
                      << "  -vv     : Trace level - shows all messages (ERROR, INFO, DEBUG, TRACE)\n"
                      << "\nRole options:\n"
                      << "  --endpoint, -e  : Run as endpoint\n"
                      << "  --bridge, -b    : Run as bridge\n"
                      << "  --discovery, -d : Discovery identifier (required)\n";
            std::exit(0);
        }

        const bool is_endpoint = variables.count("endpoint") != 0U;
        const bool is_bridge   = variables.count("bridge") != 0U;
        if (is_endpoint == is_bridge)
        {
            throw po::error("Exactly one of --endpoint/--bridge must be specified.");
        }

        config.endpoint_mode    = is_endpoint ? EndpointMode::endpoint : EndpointMode::bridge;
        config.discovery_target = variables["discovery"].as<std::string>();

        if (verbosity == 0)
        {
            config.log_level = logging::LogLevel::Info;
        }
        else if (verbosity == 1)
        {
            config.log_level = logging::LogLevel::Debug;
        }
        else
        {
            config.log_level = logging::LogLevel::Trace;
        }

        EESTV_LOG_INFO("DataBridge instance created");
        EESTV_LOG_DEBUG("Verbosity level: " << verbosity);
        EESTV_LOG_DEBUG("Endpoint mode: " << (is_endpoint ? "endpoint" : "bridge"));
        EESTV_LOG_INFO("Discovery target: " << config.discovery_target);
        EESTV_LOG_TRACE("Command line arguments parsed successfully");
    }
    catch (const po::error& error)
    {
        EESTV_LOG_ERROR("Error parsing command line: " << error.what());
        std::cerr << "Error: " << error.what() << '\n';
        std::cerr << desc << '\n';
        throw;
    }

    return config;
}

} // namespace eestv