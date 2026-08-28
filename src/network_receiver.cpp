#include "network_receiver.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#if defined(__x86_64__)
#include <immintrin.h>
#endif

NetworkReceiver::NetworkReceiver(uint16_t port, SpscQueue<IngestOrderCommand, 131072>& queue)
    : port_(port), queue_(queue) {
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    socket_tune(sockfd_);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(sockfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("Bind failed");
        close(sockfd_);
        exit(EXIT_FAILURE);
    }
}

NetworkReceiver::~NetworkReceiver() {
    stop();
    if (sockfd_ >= 0)
        close(sockfd_);
}

void NetworkReceiver::socket_tune(int fd) {
    int rcvbuf_size = 64 * 1024 * 1024;  // 64MB buffer
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size));

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    int busy_poll_us = 50;  // Busy polling low-latency
    setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll_us, sizeof(busy_poll_us));
}

void NetworkReceiver::start() {
    is_running_.store(true, std::memory_order_release);
    worker_thread_ = std::thread(&NetworkReceiver::receive_loop, this);
}

void NetworkReceiver::stop() {
    if (is_running_.exchange(false, std::memory_order_acq_rel)) {
        if (worker_thread_.joinable())
            worker_thread_.join();
    }
}

void NetworkReceiver::receive_loop() {
    struct mmsghdr msgs[NETWORK_BATCH_SIZE];
    struct iovec iovecs[NETWORK_BATCH_SIZE];
    IngestOrderCommand packets[NETWORK_BATCH_SIZE];
    sockaddr_in client_addrs[NETWORK_BATCH_SIZE];

    std::memset(msgs, 0, sizeof(msgs));
    for (size_t i = 0; i < NETWORK_BATCH_SIZE; ++i) {
        iovecs[i].iov_base = &packets[i];
        iovecs[i].iov_len = sizeof(IngestOrderCommand);
        msgs[i].msg_hdr.msg_name = &client_addrs[i];
        msgs[i].msg_hdr.msg_namelen = sizeof(sockaddr_in);
        msgs[i].msg_hdr.msg_iov = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }

    while (is_running_.load(std::memory_order_relaxed)) {
        int num_msgs = recvmmsg(sockfd_, msgs, NETWORK_BATCH_SIZE, MSG_DONTWAIT, nullptr);
        if (num_msgs > 0) {
            for (int i = 0; i < num_msgs; ++i) {
                if (msgs[i].msg_len == sizeof(IngestOrderCommand)) {
                    while (!queue_.push(packets[i])) {
#if defined(__x86_64__)
                        _mm_pause();
#endif
                    }
                }
            }
        } else {
#if defined(__x86_64__)
            _mm_pause();
#endif
        }
    }
}