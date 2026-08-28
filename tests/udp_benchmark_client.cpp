#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>

#include "types.hpp"

constexpr size_t TOTAL_TEST_ORDERS = 1'000'000;
constexpr size_t BATCH_SIZE = 64;

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    struct mmsghdr msgs[BATCH_SIZE];
    struct iovec iovecs[BATCH_SIZE];
    IngestOrderCommand packets[BATCH_SIZE];

    std::memset(msgs, 0, sizeof(msgs));
    for (size_t i = 0; i < BATCH_SIZE; ++i) {
        iovecs[i].iov_base = &packets[i];
        iovecs[i].iov_len = sizeof(IngestOrderCommand);
        msgs[i].msg_hdr.msg_name = &server_addr;
        msgs[i].msg_hdr.msg_namelen = sizeof(server_addr);
        msgs[i].msg_hdr.msg_iov = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }

    std::cout << "Blasting " << TOTAL_TEST_ORDERS << " UDP orders to port " << UDP_PORT << "...\n";

    auto start_time = std::chrono::high_resolution_clock::now();
    size_t sent_count = 0;

    while (sent_count < TOTAL_TEST_ORDERS) {
        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            size_t id = sent_count + i + 1;
            packets[i].order_id = id;
            packets[i].account_id = 1000 + (id % 100);
            packets[i].side = (id % 2 == 0) ? Side::BUY : Side::SELL;
            packets[i].price = 12400 + static_cast<Price>((id % 20) * 10);
            packets[i].quantity = 10;
        }

        int sent = sendmmsg(sockfd, msgs, BATCH_SIZE, 0);
        if (sent > 0)
            sent_count += sent;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    close(sockfd);
    std::cout << "Client Finished in " << elapsed.count() << " seconds ("
              << (TOTAL_TEST_ORDERS / elapsed.count()) / 1e6 << " M pkts/sec)\n";

    return 0;
}