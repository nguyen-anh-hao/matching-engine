#pragma once
#include <atomic>
#include <cstddef>
#include <new>

template <typename T, size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

public:
    SpscQueue() : buffer_(new T[Capacity]) {}
    ~SpscQueue() {
        delete[] buffer_;
    }

    bool push(const T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        if (current_tail - cached_head_ >= Capacity) {
            cached_head_ = head_.load(std::memory_order_acquire);
            if (current_tail - cached_head_ >= Capacity)
                return false;
        }
        buffer_[current_tail & (Capacity - 1)] = item;
        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& val) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        if (current_head == cached_tail_) {
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (current_head == cached_tail_)
                return false;
        }
        val = buffer_[current_head & (Capacity - 1)];
        head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

private:
    T* buffer_;
    alignas(64) std::atomic<size_t> tail_{0};
    size_t cached_head_{0};
    alignas(64) std::atomic<size_t> head_{0};
    size_t cached_tail_{0};
};