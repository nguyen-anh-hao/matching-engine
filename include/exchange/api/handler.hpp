#pragma once

#include "exchange/api/metrics.hpp"
#include "exchange/core/engine.hpp"
#include "exchange/net/http.hpp"

#include <mutex>
#include <string>

namespace exchange {

// Translates HTTP requests into engine operations and renders JSON responses.
// Engine access is serialized through the mutex owned by HttpServer.
class ApiHandler {
public:
    ApiHandler(MatchingEngine& engine, std::mutex& engine_mutex, Metrics& metrics);

    void dispatch(const http::Request& request, int& status, std::string& body, std::string& content_type);

private:
    void place_order(const http::Request& request, int& status, std::string& body);
    void cancel_order(const http::Request& request, int& status, std::string& body);
    void open_orders(const http::Request& request, int& status, std::string& body);
    void order_book(const http::Request& request, int& status, std::string& body);
    void ticker(const http::Request& request, int& status, std::string& body);
    void prometheus(std::string& body, std::string& content_type);

    MatchingEngine& engine_;
    std::mutex& engine_mutex_;
    Metrics& metrics_;
};

} // namespace exchange
