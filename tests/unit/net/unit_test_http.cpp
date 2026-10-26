// Tests for HTTP request framing and query-string handling. Pure parsing only,
// so no socket is required.
#include "exchange/net/http.hpp"

#include "support/test_util.hpp"

#include <string>

using namespace exchange::http;

int main() {
    const auto get = parse_request("GET /book?symbol=AAPL&depth=2 HTTP/1.1\r\nHost: x\r\n\r\n");
    CHECK(get.error_status == 0);
    CHECK(get.request.method == "GET");
    CHECK(get.request.path == "/book");
    CHECK(get.request.query == "symbol=AAPL&depth=2");
    CHECK(get.request.body.empty());

    const std::string body = R"({"symbol":"AAPL"})";
    const auto post = parse_request(
        "POST /orders HTTP/1.1\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body);
    CHECK(post.error_status == 0);
    CHECK(post.request.method == "POST");
    CHECK(post.request.path == "/orders");
    CHECK(post.request.body == body);

    CHECK(parse_request("GET /health HTTP/1.0\r\n\r\n").error_status == 0);

    // Framing errors
    CHECK(parse_request("GET / HTTP/1.1").error_status == 400); // no CRLFCRLF
    CHECK(parse_request("GET / HTTP/1.1\r\nBadHeader\r\n\r\n").error_status == 400);
    CHECK(parse_request("GET / HTTP/1.1\r\nContent-Length: x\r\n\r\n").error_status == 400);
    CHECK(parse_request("GET / HTTP/1.1\r\nContent-Length: 10\r\n\r\nshort").error_status == 400);
    CHECK(parse_request("GET / HTTP/1.1\r\nContent-Length: 2\r\n\r\nabc").error_status == 0);
    CHECK(parse_request("GET / HTTP/1.1\r\nContent-Length: 2\r\nContent-Length: 3\r\n\r\nabc").error_status ==
          400);
    CHECK(parse_request("GET / HTTP/1.1\r\nContent-Length: 100000\r\n\r\n").error_status == 413);
    CHECK(parse_request("GET / HTTP/2.0\r\n\r\n").error_status == 400);
    CHECK(parse_request("G3T / HTTP/1.1\r\n\r\n").error_status == 400);
    CHECK(parse_request("GET  HTTP/1.1\r\n\r\n").error_status == 400);
    CHECK(parse_request("GET relative HTTP/1.1\r\n\r\n").error_status == 400);

    std::string oversized_headers = "GET / HTTP/1.1\r\nX: ";
    oversized_headers.append(20'000, 'a');
    oversized_headers += "\r\n\r\n";
    CHECK(parse_request(oversized_headers).error_status == 431);

    // Query string decoding
    const std::string query = "symbol=AAPL&depth=2";
    CHECK(query_param(query, "symbol").value() == "AAPL");
    CHECK(query_param(query, "depth").value() == "2");
    CHECK(!query_param(query, "missing").has_value());
    CHECK(query_param("symbol=BRK.B%2FUSD", "symbol").value() == "BRK.B/USD");
    CHECK(query_param("symbol=A+B", "symbol").value() == "A B");
    CHECK(query_param("symbol=", "symbol").value().empty());
    CHECK(!query_param("xsymbol=AAPL", "symbol").has_value());
    CHECK(!query_param("", "symbol").has_value());

    return test::summary();
}
