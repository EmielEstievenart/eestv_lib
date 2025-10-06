add_library(
  eestv_lib
  eestv/net/discoverable_tcp_socket.cpp
  eestv/net/discovering_tcp_socket.cpp
  eestv/net/discovery/discoverable.cpp
  eestv/net/discovery/discoverable.hpp
  eestv/net/discovery/udp_discovery_server.cpp
  eestv/net/discovery/udp_discovery_server.hpp
  eestv/net/discovery/udp_discovery_client.cpp
  eestv/net/discovery/udp_discovery_client.hpp
  eestv/net/tcp_connection.cpp
  eestv/net/tcp_connection.hpp
  eestv/threading/spinlock_mutex.hpp
  eestv/data/linear_buffer.hpp
  eestv/data/linear_buffer.cpp
  eestv/data_bridge/data_bridge.hpp
  eestv/data_bridge/data_bridge.cpp
  eestv/logging/eestv_logging.hpp
  eestv/logging/eestv_logging.cpp)

target_include_directories(eestv_lib PUBLIC .)

target_link_libraries(eestv_lib PUBLIC Boost::asio Boost::system)
