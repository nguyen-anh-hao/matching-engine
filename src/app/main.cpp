#include "exchange/core/engine.hpp"
#include "exchange/server/http_server.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <thread>

namespace {

constexpr std::uint16_t kDefaultPort = 9001;
volatile std::sig_atomic_t g_stop_requested = 0;

} // namespace

extern "C" void handle_stop_signal(int) {
    g_stop_requested = 1;
}

namespace {

std::optional<std::uint16_t> parse_port(int argc, char** argv) {
    if (argc <= 1)
        return kDefaultPort;

    char* end = nullptr;
    const unsigned long value = std::strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || value == 0 || value > 65535UL) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(value);
}

void configure_logging() {
    spdlog::init_thread_pool(8192, 1);
    auto logger = spdlog::create_async<spdlog::sinks::stdout_color_sink_mt>("matching-engine");
    spdlog::set_default_logger(logger);
    logger->set_pattern(
        R"({"timestamp":"%Y-%m-%dT%H:%M:%S.%eZ","level":"%l","service_name":"matching-engine","message":"%v"})");
    spdlog::flush_on(spdlog::level::warn);

#ifdef _MSC_VER
    char* level = nullptr;
    size_t level_size = 0;
    if (::_dupenv_s(&level, &level_size, "LOG_LEVEL") == 0 && level != nullptr) {
        spdlog::set_level(spdlog::level::from_str(level));
        std::free(level);
    }
#else
    if (const char* level = std::getenv("LOG_LEVEL"); level != nullptr) {
        spdlog::set_level(spdlog::level::from_str(level));
    }
#endif
}

} // namespace

int main(int argc, char** argv) {
    using namespace exchange;

    configure_logging();
    const auto logger = spdlog::default_logger();

    const auto port = parse_port(argc, argv);
    if (!port.has_value()) {
        logger->error("invalid port argument: {}", argc > 1 ? argv[1] : "");
        spdlog::shutdown();
        return 2;
    }

    std::signal(SIGINT, handle_stop_signal);
    std::signal(SIGTERM, handle_stop_signal);

    MatchingEngine engine;
    HttpServer server(*port, engine);

    std::atomic<bool> finished{false};
    bool started = false;
    std::thread server_thread([&server, &finished, &started] {
        started = server.run();
        finished.store(true, std::memory_order_release);
    });

    while (g_stop_requested == 0 && !finished.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.stop();
    server_thread.join();
    logger->info("matching-engine stopped");
    spdlog::shutdown();
    return started ? 0 : 1;
}
