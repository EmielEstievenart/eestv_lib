#include "eestv/data_bridge/data_bridge.hpp"
#include "eestv/logging/eestv_logging.hpp"

#include <boost/program_options.hpp>
#include <iostream>

namespace eestv
{

DataBridge::DataBridge(int argc, char* argv[])
{
    // Default log level is Info (shows Error and Info)
    EESTV_SET_LOG_LEVEL(Info);

    parse_command_line_parameters(argc, argv);
}

void DataBridge::parse_command_line_parameters(int argc, char* argv[])
{
    // Parse command line options
    namespace po = boost::program_options;

    po::options_description desc("DataBridge Options");
    desc.add_options()("help,h", "Show help message");

    // Count verbosity level from -v, -vv, -vvv format
    int verbosity = 0;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.size() >= 2 && arg[0] == '-' && arg[1] == 'v')
        {
            // Count consecutive 'v' characters after the '-'
            size_t v_count = 0;
            for (size_t j = 1; j < arg.size() && arg[j] == 'v'; ++j)
            {
                v_count++;
            }
            // Only accept if the entire arg is just "-v", "-vv", "-vvv", etc.
            if (v_count == arg.size() - 1)
            {
                verbosity = static_cast<int>(v_count);
                break; // Only use the first verbosity flag found
            }
        }
    }

    po::variables_map vm;

    try
    {
        // Use command_line_parser to allow unregistered options (like -v, -vv)
        po::store(po::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
        po::notify(vm);

        // Handle help option
        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            std::cout << "\nVerbosity levels:\n"
                      << "  (none)  : Info level  - shows ERROR and INFO messages\n"
                      << "  -v      : Debug level - shows ERROR, INFO, and DEBUG messages\n"
                      << "  -vv     : Trace level - shows all messages (ERROR, INFO, DEBUG, TRACE)\n"
                      << "\nUsage: data_bridge [options]\n"
                      << "       data_bridge -v\n"
                      << "       data_bridge -vv\n";
            return;
        }

        // Set log level based on verbosity count
        if (verbosity == 0)
        {
            EESTV_SET_LOG_LEVEL(Info); // Default: Error + Info
        }
        else if (verbosity == 1)
        {
            EESTV_SET_LOG_LEVEL(Debug); // -v: Error + Info + Debug
        }
        else if (verbosity >= 2)
        {
            EESTV_SET_LOG_LEVEL(Trace); // -vv or more: All levels
        }

        EESTV_LOG_INFO("DataBridge instance created");
        EESTV_LOG_DEBUG("Verbosity level: " << verbosity);
        EESTV_LOG_TRACE("Command line arguments parsed successfully");
    }
    catch (const po::error& e)
    {
        EESTV_LOG_ERROR("Error parsing command line: " << e.what());
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << desc << std::endl;
    }
}

} // namespace eestv