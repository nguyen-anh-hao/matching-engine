#include <arpa/inet.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

constexpr int TOTAL_ORDERS = 100'000;

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    std::cout << "Sending " << TOTAL_ORDERS << " orders to Naive Engine...\n";

    char recv_buf[128];
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 1; i <= TOTAL_ORDERS; ++i) {
        double price = 124.0 + (i % 5) * 0.5;
        char side = (i % 2 == 0) ? 'B' : 'S';

        std::string msg =
            "NEW " + std::to_string(i) + " 1001 2 " + side + " " + std::to_string(price) + " 10\n";

        write(sock, msg.c_str(), msg.length());
        read(sock, recv_buf, sizeof(recv_buf));
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    close(sock);

    std::cout << "\n--- NAIVE BENCHMARK RESULTS ---\n";
    std::cout << "Elapsed Time : " << elapsed.count() << " seconds\n";
    std::cout << "Throughput   : " << TOTAL_ORDERS / elapsed.count() << " orders/sec\n";
    std::cout << "Avg Latency  : " << (elapsed.count() * 1e6) / TOTAL_ORDERS << " µs / order\n";

    return 0;
}