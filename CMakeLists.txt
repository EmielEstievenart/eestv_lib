add_library(
  eestv_lib
  # discoverable/discovering sockets removed
  eestv/net/discovery/discoverable.cpp
  eestv/net/discovery/discoverable.hpp
  eestv/net/discovery/udp_discovery_server.cpp
  eestv/net/discovery/udp_discovery_server.hpp
  eestv/net/discovery/udp_discovery_client.cpp
  eestv/net/discovery/udp_discovery_client.hpp
  eestv/net/connection/tcp_connection.hpp
  eestv/net/connection/tcp_client_connection.hpp
  eestv/net/connection/tcp_server_connection.hpp
  eestv/net/connection/tcp_server.hpp
  eestv/threading/spinlock_mutex.hpp
  eestv/data/linear_buffer.hpp
  eestv/data/linear_buffer.cpp
  eestv/data_bridge/data_bridge.hpp
  eestv/data_bridge/data_bridge.cpp
  eestv/logging/eestv_logging.hpp
  eestv/logging/eestv_logging.cpp)

target_include_directories(eestv_lib PUBLIC .)

target_compile_definitions(eestv_lib PUBLIC BOOST_ASIO_NO_DEPRECATED)

target_link_libraries(eestv_lib PUBLIC Boost::asio Boost::system
                                       Boost::program_options)
