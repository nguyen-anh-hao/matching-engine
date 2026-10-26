#include "exchange/net/http.hpp"

#include <cctype>
#include <charconv>
#include <cstddef>
#include <system_error>
#include <utility>

namespace exchange::http {
namespace {

constexpr size_t kMaxHeaderBytes = 16 * 1024;
constexpr size_t kMaxBodyBytes = 64 * 1024;

bool is_method_token(std::string_view text) {
    if (text.empty())
        return false;
    for (const char c : text) {
        if (std::isalpha(static_cast<unsigned char>(c)) == 0 && c != '-')
            return false;
    }
    return true;
}

bool iequals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size())
        return false;
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
            std::tolower(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }
    return true;
}

std::optional<size_t> declared_body_length(std::string_view headers) {
    size_t position = headers.find("\r\n");
    if (position == std::string_view::npos)
        return std::nullopt;
    position += 2;

    std::optional<size_t> content_length;
    while (position < headers.size()) {
        const size_t line_end = headers.find("\r\n", position);
        if (line_end == std::string_view::npos)
            return std::nullopt;

        const std::string_view line = headers.substr(position, line_end - position);
        position = line_end + 2;
        if (line.empty())
            break;

        const size_t colon = line.find(':');
        if (colon == std::string_view::npos)
            return std::nullopt;
        if (!iequals(line.substr(0, colon), "content-length"))
            continue;

        if (content_length.has_value())
            return std::nullopt;

        std::string_view value = line.substr(colon + 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.remove_prefix(1);
        }
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
            value.remove_suffix(1);
        }
        if (value.empty())
            return std::nullopt;

        size_t parsed = 0;
        const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
            return std::nullopt;
        }
        content_length = parsed;
    }
    return content_length.value_or(0);
}

const char* reason_phrase(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 408:
        return "Request Timeout";
    case 413:
        return "Payload Too Large";
    case 422:
        return "Unprocessable Entity";
    case 431:
        return "Request Header Fields Too Large";
    case 500:
        return "Internal Server Error";
    case 503:
        return "Service Unavailable";
    default:
        return "Error";
    }
}

std::string url_decode(std::string_view text) {
    std::string decoded;
    decoded.reserve(text.size());

    const auto hex_value = [](char digit) -> int {
        if (digit >= '0' && digit <= '9')
            return digit - '0';
        if (digit >= 'a' && digit <= 'f')
            return digit - 'a' + 10;
        if (digit >= 'A' && digit <= 'F')
            return digit - 'A' + 10;
        return -1;
    };

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '+') {
            decoded.push_back(' ');
            continue;
        }
        if (text[i] == '%' && i + 2 < text.size()) {
            const int high = hex_value(text[i + 1]);
            const int low = hex_value(text[i + 2]);
            if (high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>((high << 4) | low));
                i += 2;
                continue;
            }
        }
        decoded.push_back(text[i]);
    }
    return decoded;
}

} // namespace

void send_response(socket_t socket, int status, std::string_view body, std::string_view content_type) {
    if (socket == kInvalidSocket)
        return;

    std::string header = "HTTP/1.1 " + std::to_string(status) + " " + reason_phrase(status) + "\r\n";
    header += "Content-Type: ";
    header += content_type;
    header += "\r\nContent-Length: ";
    header += std::to_string(body.size());
    header += "\r\nConnection: close\r\n\r\n";

    if (!net::send_all(socket, header))
        return;
    net::send_all(socket, body);
}

RequestParseResult parse_request(std::string_view raw) {
    const size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string_view::npos)
        return {{}, 400};

    const size_t header_size = header_end + 4;
    if (header_size > kMaxHeaderBytes)
        return {{}, 431};

    const auto declared = declared_body_length(raw.substr(0, header_size));
    if (!declared.has_value())
        return {{}, 400};
    if (*declared > kMaxBodyBytes)
        return {{}, 413};
    if (raw.size() < header_size + *declared)
        return {{}, 400};

    const size_t line_end = raw.find("\r\n");
    if (line_end == std::string_view::npos)
        return {{}, 400};

    const std::string_view request_line = raw.substr(0, line_end);
    const size_t first_space = request_line.find(' ');
    const size_t last_space = request_line.rfind(' ');
    if (first_space == std::string_view::npos || last_space == first_space)
        return {{}, 400};

    const std::string_view method = request_line.substr(0, first_space);
    const std::string_view target = request_line.substr(first_space + 1, last_space - first_space - 1);
    const std::string_view version = request_line.substr(last_space + 1);
    if (!is_method_token(method) || target.empty() || target.front() != '/' ||
        (version != "HTTP/1.0" && version != "HTTP/1.1")) {
        return {{}, 400};
    }

    Request request;
    request.method = std::string(method);
    const size_t query_start = target.find('?');
    request.path = std::string(target.substr(0, query_start));
    if (query_start != std::string_view::npos) {
        request.query = std::string(target.substr(query_start + 1));
    }
    request.body = std::string(raw.substr(header_size, *declared));
    return {std::move(request), 0};
}

RequestParseResult read_request(socket_t socket) {
    if (socket == kInvalidSocket)
        return {{}, 400};

    std::string raw;
    char buffer[8192];
    size_t header_size = 0;
    for (;;) {
        int received = 0;
        if (!net::recv_some(socket, buffer, sizeof(buffer), received))
            return {{}, 408};
        if (received == 0)
            return {{}, 400};
        raw.append(buffer, static_cast<size_t>(received));

        const size_t end = raw.find("\r\n\r\n");
        if (end != std::string::npos) {
            header_size = end + 4;
            break;
        }
        if (raw.size() > kMaxHeaderBytes)
            return {{}, 431};
    }
    if (header_size > kMaxHeaderBytes)
        return {{}, 431};

    const auto declared = declared_body_length(std::string_view(raw).substr(0, header_size));
    if (!declared.has_value())
        return {{}, 400};
    if (*declared > kMaxBodyBytes)
        return {{}, 413};

    const size_t total = header_size + *declared;
    while (raw.size() < total) {
        int received = 0;
        if (!net::recv_some(socket, buffer, sizeof(buffer), received))
            return {{}, 408};
        if (received == 0)
            return {{}, 400};
        raw.append(buffer, static_cast<size_t>(received));
    }
    if (raw.size() > total)
        raw.resize(total);
    return parse_request(raw);
}

std::optional<std::string> query_param(std::string_view query, std::string_view key) {
    size_t position = 0;
    while (position <= query.size()) {
        const size_t separator = query.find('&', position);
        const std::string_view pair = query.substr(
            position, separator == std::string_view::npos ? std::string_view::npos : separator - position);
        const size_t equals = pair.find('=');
        const std::string_view name = pair.substr(0, equals);
        if (url_decode(name) == key) {
            return equals == std::string_view::npos ? std::string{} : url_decode(pair.substr(equals + 1));
        }
        if (separator == std::string_view::npos)
            break;
        position = separator + 1;
    }
    return std::nullopt;
}

} // namespace exchange::http
