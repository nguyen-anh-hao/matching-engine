# Matching Engine (`matching-engine`)

> **Ultra-Low-Latency Order Matching Core** written in modern C++ (C++20). Deterministic in-memory price-time priority (FIFO) execution.

---

## 1. Primary Responsibility
The Matching Engine maintains the live Limit Order Book (LOB) for active instruments. It executes incoming limit and market orders deterministically, computes trade fills, updates resting book depth, and broadcasts execution reports.

**Rule Zero:** The engine performs **ZERO disk I/O, ZERO database interactions, and ZERO blocking syscalls** on the critical matching path.

---

## 2. Boundaries & I/O
- **Ingress (Orders in):** Accepts order requests from `api-gateway` (or dedicated Risk gateway).
- **Egress (Trades out):** Emits `ExecutionReport` (fills, partial fills, cancels) to `clearing-settlement` and `api-gateway`.
- **Contract Reference:** `proto/engine/order_book.proto` & `proto/engine/execution_report.proto`.

---

## 3. Quick Start (Build & Test)

### Prerequisites
- CMake 3.20+
- Modern C++ Compiler (`gcc-12+` or `clang-15+` or MSVC 2022)

```bash
# Configure and build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Run native unit tests & benchmarks
cd build && ctest --output-on-failure
```

### Run with Docker (Isolated)
```bash
docker build -t matching-engine:local .
docker run -p 9001:9001 matching-engine:local
```

---

## 4. Engineering Roadmap (Hardware & Latency Optimization)

| Version | Phase | Target Metric & Engineering Problem |
| :--- | :--- | :--- |
| **v1.0.0** *(Current)* | **Functional Baseline** | In-memory Price-Time Priority LOB. HTTP REST adapter for end-to-end prototyping. Latency ~ 200–500 microseconds. |
| **v1.1.0** | **gRPC / Binary Transport** | Replace HTTP server with asynchronous gRPC server (`grpc++`) using completion queues. Eliminate JSON parsing overhead. |
| **v1.2.0** | **Cache-Locality & Lock-Free Structures** | Refactor price-level index from `std::map`/pointer-based tree to contiguous array / cache-aligned ring buffers (`folly::MPMCQueue` or LMAX Disruptor pattern). Eliminate dynamic heap allocation (`malloc`/`new`) on the hot path via custom arena allocators. Target: P99 < 15 microseconds. |
| **v2.0.0** | **Deterministic Replication & Kernel Bypass** | Integrate Raft/Aeron cluster consensus for active-passive hot-failover without state loss. Implement POSIX shared memory (`shm_open`) or Solarflare EF_VI / DPDK kernel bypass for sub-microsecond tick-to-trade latency. |
