#include "net.hpp"

#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace exchange::net {
namespace {

#ifdef _WIN32
using native_socket = SOCKET;
constexpr native_socket kNativeInvalid = INVALID_SOCKET;

native_socket to_native(socket_t socket) {
    return static_cast<native_socket>(socket);
}

bool is_invalid(native_socket socket) {
    return socket == kNativeInvalid;
}

void close_native(native_socket socket) {
    ::closesocket(socket);
}

int send_native(native_socket socket, const char* data, int length) {
    return ::send(socket, data, length, 0);
}

int recv_native(native_socket socket, char* data, int length) {
    return ::recv(socket, data, length, 0);
}

bool was_interrupted() {
    return ::WSAGetLastError() == WSAEINTR;
}
#else
using native_socket = int;
constexpr native_socket kNativeInvalid = -1;

native_socket to_native(socket_t socket) {
    return static_cast<native_socket>(socket);
}

bool is_invalid(native_socket socket) {
    return socket == kNativeInvalid;
}

void close_native(native_socket socket) {
    ::close(socket);
}

int send_native(native_socket socket, const char* data, int length) {
    return static_cast<int>(::send(socket, data, static_cast<size_t>(length), 0));
}

int recv_native(native_socket socket, char* data, int length) {
    return static_cast<int>(::recv(socket, data, static_cast<size_t>(length), 0));
}

bool was_interrupted() {
    return errno == EINTR;
}
#endif

} // namespace

bool init() {
#ifdef _WIN32
    static const bool ready = [] {
        WSADATA data{};
        return ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return ready;
#else
    return true;
#endif
}

void shutdown() {
#ifdef _WIN32
    ::WSACleanup();
#endif
}

void close_socket(socket_t socket) {
    if (socket == kInvalidSocket)
        return;
    close_native(to_native(socket));
}

bool set_timeout(socket_t socket, int timeout_ms) {
    if (socket == kInvalidSocket)
        return false;
    const native_socket handle = to_native(socket);
#ifdef _WIN32
    const DWORD timeout = static_cast<DWORD>(timeout_ms);
    return ::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
                        sizeof(timeout)) == 0 &&
           ::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout),
                        sizeof(timeout)) == 0;
#else
    const timeval timeout{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    return ::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
           ::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
#endif
}

socket_t listen_tcp(std::uint16_t port) {
    const native_socket handle = ::socket(AF_INET, SOCK_STREAM, 0);
    if (is_invalid(handle))
        return kInvalidSocket;

    int reuse = 1;
    ::setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (::bind(handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(handle, SOMAXCONN) != 0) {
        close_native(handle);
        return kInvalidSocket;
    }
    return static_cast<socket_t>(handle);
}

bool wait_readable(socket_t socket, int timeout_ms) {
    if (socket == kInvalidSocket)
        return false;
    const native_socket handle = to_native(socket);

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(handle, &read_set);

    timeval timeout{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
#ifdef _WIN32
    return ::select(0, &read_set, nullptr, nullptr, &timeout) > 0;
#else
    return ::select(handle + 1, &read_set, nullptr, nullptr, &timeout) > 0;
#endif
}

socket_t accept_client(socket_t listen_socket) {
    if (listen_socket == kInvalidSocket)
        return kInvalidSocket;

    sockaddr_in peer{};
#ifdef _WIN32
    int peer_length = sizeof(peer);
#else
    socklen_t peer_length = sizeof(peer);
#endif
    const native_socket handle =
        ::accept(to_native(listen_socket), reinterpret_cast<sockaddr*>(&peer), &peer_length);
    if (is_invalid(handle))
        return kInvalidSocket;

    const auto client = static_cast<socket_t>(handle);
    set_timeout(client, kSocketTimeoutMs);
    return client;
}

bool send_all(socket_t socket, std::string_view data) {
    if (socket == kInvalidSocket)
        return false;
    const native_socket handle = to_native(socket);

    size_t sent = 0;
    while (sent < data.size()) {
        const size_t remaining = data.size() - sent;
        const int chunk = static_cast<int>(std::min<size_t>(remaining, 1u << 20));
        const int written = send_native(handle, data.data() + sent, chunk);
        if (written > 0) {
            sent += static_cast<size_t>(written);
            continue;
        }
        if (written < 0 && was_interrupted())
            continue;
        return false;
    }
    return true;
}

bool recv_some(socket_t socket, char* buffer, std::size_t capacity, int& received) {
    received = 0;
    if (socket == kInvalidSocket)
        return false;
    const native_socket handle = to_native(socket);
    const int length = static_cast<int>(std::min<std::size_t>(capacity, 1u << 20));

    for (;;) {
        const int result = recv_native(handle, buffer, length);
        if (result >= 0) {
            received = result;
            return true;
        }
        if (!was_interrupted())
            return false;
    }
}

} // namespace exchange::net
