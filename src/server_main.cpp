#include <netinet/in.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "matching_engine.hpp"

constexpr int PORT = 8888;

void handle_client(int client_fd, MatchingEngine& engine) {
    char buffer[1024];

    while (true) {
        std::memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0)
            break;

        std::istringstream iss(buffer);
        std::string command;
        iss >> command;

        if (command == "NEW") {
            Order order;
            char side_char;
            iss >> order.order_id >> order.account_id >> order.symbol_id >> side_char >>
                order.price >> order.initial_quantity;

            order.side = static_cast<Side>(side_char);
            order.remaining_quantity = order.initial_quantity;
            order.timestamp_ns = 0;

            auto trades = engine.process_order(order);

            std::string response = "ACK " + std::to_string(trades.size()) + " TRADES\n";
            write(client_fd, response.c_str(), response.length());
        }
    }
    close(client_fd);
}

int main() {
    MatchingEngine engine("naive_wal.log");
    
    engine.register_symbol(1);  // AAPL
    engine.register_symbol(2);  // NVDA
    engine.register_symbol(3);  // BTC
    engine.register_symbol(4);  // ETH

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*) &address, sizeof(address));
    listen(server_fd, 10);

    std::cout << "Naive Matching Engine running on port " << PORT << "...\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0) {
            std::thread(handle_client, client_fd, std::ref(engine)).detach();
        }
    }

    close(server_fd);
    return 0;
}