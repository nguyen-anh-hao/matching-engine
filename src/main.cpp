#include "engine.hpp"
#include "server.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    using namespace exchange;
    uint16_t port = 9001;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::strtoul(argv[1], nullptr, 10));
    }

    MatchingEngine engine;
    HttpServer server(port, engine);
    std::cout << "[matching-engine] listening on :" << port << std::endl;
    server.run();
    return 0;
}
