#pragma once

#include "exchange/net/socket.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace exchange::http {

using socket_t = net::socket_t;
inline constexpr socket_t kInvalidSocket = net::kInvalidSocket;

// A parsed HTTP/1.x request. `path` excludes the query string, which is kept
// raw in `query` (without the leading '?').
struct Request {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
};

// error_status is 0 on success, otherwise the HTTP status to answer with.
struct RequestParseResult {
    Request request;
    int error_status = 0;
};

void send_response(socket_t socket, int status, std::string_view body,
                   std::string_view content_type = "application/json");

// Reads one request from a client. Enforces header/body size limits and socket
// timeouts; returns an error status instead of throwing.
RequestParseResult read_request(socket_t socket);

// Parses an already received request. Exposed separately so the HTTP framing
// can be unit tested without a socket.
RequestParseResult parse_request(std::string_view raw);

// Looks up a query parameter, percent-decoding both name and value ('+' becomes
// a space). Returns nullopt when the parameter is absent; an empty string means
// the parameter was present without a value.
std::optional<std::string> query_param(std::string_view query, std::string_view key);

} // namespace exchange::http
