#include "exchange/server/http_server.hpp"

#include "exchange/api/serialization.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <exception>
#include <string>
#include <thread>
#include <vector>

namespace exchange {

HttpServer::HttpServer(std::uint16_t port, MatchingEngine& engine)
    : port_(port), engine_(engine), metrics_(), api_(engine, engine_mutex_, metrics_) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::stop() {
    running_.store(false, std::memory_order_relaxed);
}

bool HttpServer::run() {
    if (!net::init()) {
        SPDLOG_ERROR("network stack initialisation failed");
        return false;
    }

    const net::socket_t listen_socket = net::listen_tcp(port_);
    if (listen_socket == net::kInvalidSocket) {
        SPDLOG_ERROR("failed to listen on :{}", port_);
        net::shutdown();
        return false;
    }

    const size_t worker_count =
        std::clamp<size_t>(std::thread::hardware_concurrency(), kMinWorkers, kMaxWorkers);
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (size_t i = 0; i < worker_count; ++i) {
        workers.emplace_back([this] { worker_loop(); });
    }
    SPDLOG_INFO("matching-engine listening on :{} with {} workers", port_, worker_count);

    while (running_.load(std::memory_order_relaxed)) {
        if (!net::wait_readable(listen_socket, 250))
            continue;

        const net::socket_t client = net::accept_client(listen_socket);
        if (client == net::kInvalidSocket) {
            if (running_.load(std::memory_order_relaxed))
                SPDLOG_WARN("accept() failed");
            continue;
        }

        bool queued = false;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (queue_.size() < kMaxQueuedConnections) {
                queue_.push_back(client);
                queued = true;
            }
        }

        if (!queued) {
            http::send_response(client, 503, api_detail::error_body(503), "application/json");
            net::close_socket(client);
            metrics_.record_throttled();
            continue;
        }
        queue_cv_.notify_one();
    }
    net::close_socket(listen_socket);

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stopping_ = true;
    }
    queue_cv_.notify_all();
    for (auto& worker : workers)
        worker.join();

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        for (const net::socket_t socket : queue_) {
            net::close_socket(socket);
        }
        queue_.clear();
    }
    net::shutdown();
    return true;
}

void HttpServer::worker_loop() {
    for (;;) {
        net::socket_t client = net::kInvalidSocket;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty())
                return;
            client = queue_.front();
            queue_.pop_front();
        }

        try {
            handle_connection(client);
        } catch (const std::exception& error) {
            SPDLOG_ERROR("request failed: {}", error.what());
            http::send_response(client, 500, api_detail::error_body(500), "application/json");
        } catch (...) {
            SPDLOG_ERROR("request failed with an unknown exception");
            http::send_response(client, 500, api_detail::error_body(500), "application/json");
        }
        net::close_socket(client);
    }
}

void HttpServer::handle_connection(net::socket_t client) {
    metrics_.begin_connection();
    struct ConnectionGuard {
        Metrics& metrics;
        ~ConnectionGuard() { metrics.end_connection(); }
    } guard{metrics_};

    const http::RequestParseResult parsed = http::read_request(client);
    if (parsed.error_status != 0) {
        http::send_response(client, parsed.error_status, api_detail::error_body(parsed.error_status),
                            "application/json");
        return;
    }

    int status = 200;
    std::string body;
    std::string content_type = "application/json";
    api_.dispatch(parsed.request, status, body, content_type);
    http::send_response(client, status, body, content_type);
}

} // namespace exchange
