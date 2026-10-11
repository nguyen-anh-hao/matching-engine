#include "engine.hpp"
#include "server.hpp"

#include <cstdint>
#include <cstdlib>

#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    using namespace exchange;
    uint16_t port = 9001;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::strtoul(argv[1], nullptr, 10));
    }

    // Async logger keeps the matching hot path from blocking on I/O.
    spdlog::init_thread_pool(8192, 1);
    auto logger = spdlog::create_async<spdlog::sinks::stdout_color_sink_mt>("matching-engine");
    spdlog::set_default_logger(logger);
    // Structured JSON output (matching-engine logs are minimal: startup + socket errors).
    logger->set_pattern(R"({"timestamp":"%Y-%m-%dT%H:%M:%S.%eZ","level":"%l","service_name":"matching-engine","message":"%v"})");
    if (const char* lvl = std::getenv("LOG_LEVEL")) {
        spdlog::set_level(spdlog::level::from_str(lvl));
    }

    MatchingEngine engine;
    HttpServer server(port, engine);
    logger->info("matching-engine listening on :{}", port);
    server.run();
    return 0;
}
