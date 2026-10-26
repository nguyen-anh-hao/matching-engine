#include "exchange/api/handler.hpp"

#include "exchange/api/serialization.hpp"

namespace exchange {

ApiHandler::ApiHandler(MatchingEngine& engine, std::mutex& engine_mutex, Metrics& metrics)
    : engine_(engine), engine_mutex_(engine_mutex), metrics_(metrics) {}

void ApiHandler::dispatch(const http::Request& request, int& status, std::string& body,
                          std::string& content_type) {
    status = 200;
    const auto method_not_allowed = [&status, &body] {
        status = 405;
        body = api_detail::error_body(405);
    };

    if (request.path == "/health") {
        if (request.method != "GET") {
            method_not_allowed();
            return;
        }
        body = R"({"status":"ok"})";
        return;
    }

    if (request.path == "/metrics") {
        if (request.method != "GET") {
            method_not_allowed();
            return;
        }
        prometheus(body, content_type);
        return;
    }

    if (request.path == "/orders") {
        if (request.method == "POST") {
            place_order(request, status, body);
            return;
        }
        if (request.method == "GET") {
            open_orders(request, status, body);
            return;
        }
        if (request.method == "DELETE") {
            cancel_order(request, status, body);
            return;
        }
        method_not_allowed();
        return;
    }

    if (request.path == "/book") {
        if (request.method != "GET") {
            method_not_allowed();
            return;
        }
        order_book(request, status, body);
        return;
    }

    if (request.path == "/ticker") {
        if (request.method != "GET") {
            method_not_allowed();
            return;
        }
        ticker(request, status, body);
        return;
    }

    status = 404;
    body = api_detail::error_body(404);
}

} // namespace exchange
