#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace exchange::net {

// Platform-neutral socket handle. The concrete type (SOCKET on Windows, int on
// POSIX) stays private to net.cpp so the rest of the codebase never includes
// <winsock2.h>.
using socket_t = std::intptr_t;

inline constexpr socket_t kInvalidSocket = -1;
inline constexpr int kSocketTimeoutMs = 5000;

// One-time network stack initialisation (WSAStartup on Windows, no-op on POSIX).
bool init();
void shutdown();

bool set_timeout(socket_t socket, int timeout_ms);

// Creates a listening socket bound to `port`. Returns kInvalidSocket on failure.
socket_t listen_tcp(std::uint16_t port);

// Waits up to `timeout_ms` for a pending connection; false means timeout/error.
bool wait_readable(socket_t socket, int timeout_ms);

// Accepts one client, applying the receive/send timeout.
socket_t accept_client(socket_t listen_socket);

void close_socket(socket_t socket);

// Sends the whole buffer; retries when a signal interrupts the call.
bool send_all(socket_t socket, std::string_view data);

// Reads up to `capacity` bytes. Returns false on transport error or timeout;
// `received` is 0 when the peer closed the connection.
bool recv_some(socket_t socket, char* buffer, std::size_t capacity, int& received);

} // namespace exchange::net
