# matching-engine

## Purpose

The deterministic order matching core: it maintains the live limit order book per symbol and turns incoming orders into trades. It is the only stateful service by design, because the book itself is the authoritative state. It has no database, no disk writes and no durable history of its own.

## Domain knowledge

Prices and quantities are signed 64-bit fixed-point integers scaled by one hundred million, so price comparison is an integer comparison and rounding never enters the matching path. Orders match best price first and, within a price level, oldest first, which is strict price-time priority. A limit order never trades through its limit, while a market order accepts any available price.

Self-trade prevention skips resting orders that belong to the same account instead of cancelling them, so an aggressive order can cross its own liquidity without generating a trade. Good-till-cancelled orders rest after partial execution, immediate-or-cancel orders drop the remainder, and fill-or-kill orders are rejected unless the book can fill them completely before any trade occurs. A trade always pairs one resting maker with one incoming taker, and the maker's price sets the execution price.

Because the engine keeps no client order identifier index and exposes no recovery query, a caller cannot ask it whether an order exists after a crash. Its only durable knowledge is what its callers decided to persist elsewhere. The book and ticker endpoints are read-only projections of live memory, not history.

## Decisions and consequences

**Single-writer in-memory engine.** One process owns the book and applies orders sequentially, which removes locking and makes execution deterministic. The cost is that replicas cannot be scaled horizontally and a second replica would silently split the book.

**Price-time priority with level queues.** Fairness and predictability come before raw throughput. The current price index uses ordered maps, which is correct but cache-unfriendly; a contiguous price ladder is the planned replacement.

**Integer fixed-point arithmetic.** Monetary comparisons stay exact and fast at the cost of conversion at every boundary. Any component that reintroduces floating point into prices breaks the guarantee.

**No persistence on the hot path.** Avoiding disk and database calls keeps latency predictable, but a crash loses the book. High availability must come from snapshot and replay, not from a shared database.

## Operations and known limits

The engine must run with exactly one active writer; two replicas produce two different books and inconsistent fills. The health and metrics endpoints serve orchestration, while the book and ticker endpoints serve read-only introspection. It performs no authentication of its own and assumes it is reachable only on the internal network.

Matching is fully in memory, so memory grows with the number of resting orders and the book disappears on restart. There is no order-query API, no client-order-id deduplication and no replay endpoint; those absences are intentional and shape how callers handle unknown outcomes. Build and tests run through CMake, the binary listens on port 9001 by default, and the container exposes the same port.

## Future direction

Business: no new product capabilities are planned here; the engine stays a pure execution primitive.

Technical: gRPC and binary transport, cache-local price ladders with lock-free queues, and removal of hot-path allocation. Active-passive replication with deterministic snapshots is the intended path to high availability.

Boundaries: risk and settlement remain outside the engine, and the current HTTP adapter is expected to disappear once the binary transport lands.
