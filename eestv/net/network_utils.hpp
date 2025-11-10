#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <optional>
#include <string>

namespace eestv
{

/**
 * Determines the local IP address that would be used to reach a remote endpoint.
 *
 * This function creates a UDP socket and connects to the remote endpoint,
 * then queries the local endpoint to determine which network adapter the OS selected.
 *
 * Note: Uses synchronous socket operations, but they are all local kernel operations
 * (no network I/O) and complete essentially instantaneously.
 *
 * @param io_context The Boost.Asio io_context to use for socket operations
 * @param remote_endpoint The remote endpoint to check reachability for
 * @return The local IP address as a string, or std::nullopt if determination failed
 */
std::optional<std::string> get_local_address_for_remote(boost::asio::io_context& io_context,
                                                         const boost::asio::ip::udp::endpoint& remote_endpoint);

} // namespace eestv
