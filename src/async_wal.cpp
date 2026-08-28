#include "async_wal.hpp"

#include <iostream>

AsyncWalLogger::AsyncWalLogger(const char* filepath) {
    fd_ = open(filepath, O_CREAT | O_WRONLY | O_TRUNC | O_DIRECT, 0644);
    if (fd_ < 0) {
        fd_ = open(filepath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    }

    io_uring_queue_init(128, &ring_, 0);

    if (posix_memalign(reinterpret_cast<void**>(&aligned_buffer_), WAL_BLOCK_SIZE,
                       WAL_BLOCK_SIZE) != 0) {
        perror("posix_memalign failed");
        exit(EXIT_FAILURE);
    }
    std::memset(aligned_buffer_, 0, WAL_BLOCK_SIZE);
}

AsyncWalLogger::~AsyncWalLogger() {
    flush_pending();
    io_uring_queue_exit(&ring_);
    if (fd_ >= 0)
        close(fd_);
    free(aligned_buffer_);
}

void AsyncWalLogger::submit_buffer() {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (sqe) {
        io_uring_prep_write(sqe, fd_, aligned_buffer_, WAL_BLOCK_SIZE, write_offset_);
        io_uring_submit(&ring_);
        write_offset_ += WAL_BLOCK_SIZE;
    }
    buffer_cursor_ = 0;
    std::memset(aligned_buffer_, 0, WAL_BLOCK_SIZE);

    struct io_uring_cqe* cqe;
    while (io_uring_peek_cqe(&ring_, &cqe) == 0) {
        if (!cqe)
            break;
        io_uring_cqe_seen(&ring_, cqe);
    }
}

void AsyncWalLogger::append_trade(const TradeRecord& trade) {
    std::memcpy(aligned_buffer_ + buffer_cursor_, &trade, sizeof(TradeRecord));
    buffer_cursor_ += sizeof(TradeRecord);

    if (buffer_cursor_ >= WAL_BLOCK_SIZE) {
        submit_buffer();
    }
}

void AsyncWalLogger::flush_pending() {
    if (buffer_cursor_ > 0) {
        submit_buffer();
    }
}