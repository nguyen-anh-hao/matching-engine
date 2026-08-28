#pragma once
#include <netinet/in.h>

#include <atomic>
#include <thread>

#include "lockfree_spsc_queue.hpp"
#include "types.hpp"

class NetworkReceiver {
public:
    NetworkReceiver(uint16_t port, SpscQueue<IngestOrderCommand, 131072>& queue);
    ~NetworkReceiver();

    void start();
    void stop();

private:
    void socket_tune(int fd);
    void receive_loop();

    uint16_t port_;
    int sockfd_{-1};
    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;
    SpscQueue<IngestOrderCommand, 131072>& queue_;
};