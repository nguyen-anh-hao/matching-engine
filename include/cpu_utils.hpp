#pragma once
#include <pthread.h>
#include <sched.h>

#include <iostream>

inline bool pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        std::cerr << "Failed to pin thread to Core " << core_id << "\n";
        return false;
    }
    return true;
}