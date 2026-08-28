#pragma once
#include <fcntl.h>
#include <liburing.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>

#include "types.hpp"

class AsyncWalLogger {
public:
    explicit AsyncWalLogger(const char* filepath);
    ~AsyncWalLogger();

    void append_trade(const TradeRecord& trade);
    void flush_pending();

private:
    int fd_{-1};
    struct io_uring ring_{};
    off_t write_offset_{0};

    // Buffer căn chỉnh 4096 bytes cho Direct I/O
    char* aligned_buffer_{nullptr};
    size_t buffer_cursor_{0};

    void submit_buffer();
};