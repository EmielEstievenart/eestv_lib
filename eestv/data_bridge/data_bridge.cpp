#include "eestv/data_bridge/data_bridge.hpp"
#include "eestv/logging/eestv_logging.hpp"

#include <boost/program_options.hpp>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace eestv
{

DataBridge::DataBridge(int argc, char* argv[]) : _client_server_mode(ClientServerMode::client), _endpoint_mode(EndpointMode::endpoint)
{
    // Default log level is Info (shows Error and Info)
    EESTV_SET_LOG_LEVEL(Info);

    parse_command_line_parameters(argc, argv);
}

ClientServerMode DataBridge::client_server_mode() const noexcept
{
    return _client_server_mode;
}

EndpointMode DataBridge::endpoint_mode() const noexcept
{
    return _endpoint_mode;
}

const std::string& DataBridge::discovery_target() const noexcept
{
    return _discovery_target;
}

void DataBridge::parse_command_line_parameters(int argc, char* argv[])
{
    namespace po = boost::program_options;

    po::options_description desc("DataBridge Options");
    desc.add_options()("help,h", "Show help message")("client,c", "Run as client")("server,s", "Run as server")(
        "endpoint,e", "Run as endpoint")("bridge,b", "Run as bridge")("discovery,d", po::value<std::string>()->required(),
                                                                      "Discovery identifier (required)");

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
                      << "  --client, -c    : Run as client\n"
                      << "  --server, -s    : Run as server\n"
                      << "  --endpoint, -e  : Run as endpoint\n"
                      << "  --bridge, -b    : Run as bridge\n"
                      << "  --discovery, -d : Discovery identifier (required)\n";
            std::exit(0);
        }

        const bool is_client = variables.count("client") != 0U;
        const bool is_server = variables.count("server") != 0U;
        if (is_client == is_server)
        {
            throw po::error("Exactly one of --client/--server must be specified.");
        }

        const bool is_endpoint = variables.count("endpoint") != 0U;
        const bool is_bridge   = variables.count("bridge") != 0U;
        if (is_endpoint == is_bridge)
        {
            throw po::error("Exactly one of --endpoint/--bridge must be specified.");
        }

        _client_server_mode = is_client ? ClientServerMode::client : ClientServerMode::server;
        _endpoint_mode      = is_endpoint ? EndpointMode::endpoint : EndpointMode::bridge;
        _discovery_target   = variables["discovery"].as<std::string>();

        if (verbosity == 0)
        {
            EESTV_SET_LOG_LEVEL(Info);
        }
        else if (verbosity == 1)
        {
            EESTV_SET_LOG_LEVEL(Debug);
        }
        else
        {
            EESTV_SET_LOG_LEVEL(Trace);
        }

        EESTV_LOG_INFO("DataBridge instance created");
        EESTV_LOG_DEBUG("Verbosity level: " << verbosity);
        EESTV_LOG_DEBUG("Client/server mode: " << (is_client ? "client" : "server"));
        EESTV_LOG_DEBUG("Endpoint mode: " << (is_endpoint ? "endpoint" : "bridge"));
        EESTV_LOG_INFO("Discovery target: " << _discovery_target);
        EESTV_LOG_TRACE("Command line arguments parsed successfully");
    }
    catch (const po::error& error)
    {
        EESTV_LOG_ERROR("Error parsing command line: " << error.what());
        std::cerr << "Error: " << error.what() << '\n';
        std::cerr << desc << '\n';
        throw;
    }
}

} // namespace eestv