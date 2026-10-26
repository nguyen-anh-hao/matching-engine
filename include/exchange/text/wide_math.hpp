#pragma once

#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif

#if defined(__SIZEOF_INT128__)
// __extension__ keeps -Wpedantic quiet about the GNU 128-bit integer type.
__extension__ typedef __int128 int128_t;
#endif

namespace exchange {

// Accumulates sum(price * quantity) for post-trade reporting. A single product
// of two int64 values does not fit in 64 bits, so the accumulator is 128-bit
// where the toolchain exposes it (GCC/Clang `__int128`, MSVC x64 intrinsics).
// Bare-metal MSVC targets fall back to long double; this value is reporting
// only and never participates in matching.
class NotionalSum {
public:
    void add(int64_t price, int64_t quantity) {
        // Prices are positive for limit orders; market order trade prices come
        // from the resting maker. Guard against negative input defensively.
        if (price <= 0 || quantity <= 0)
            return;
#if defined(__SIZEOF_INT128__)
        total_ += static_cast<int128_t>(price) * static_cast<int128_t>(quantity);
#elif defined(_MSC_VER) && defined(_M_X64)
        unsigned __int64 product_high = 0;
        const unsigned __int64 product_low = _umul128(static_cast<unsigned __int64>(price),
                                                      static_cast<unsigned __int64>(quantity), &product_high);
        const unsigned char carry = _addcarry_u64(0, low_, product_low, &low_);
        _addcarry_u64(carry, high_, product_high, &high_);
#else
        total_ += static_cast<long double>(price) * static_cast<long double>(quantity);
#endif
    }

    [[nodiscard]] int64_t divide_by(int64_t divisor) const {
        if (divisor <= 0)
            return 0;
#if defined(__SIZEOF_INT128__)
        return static_cast<int64_t>(total_ / static_cast<int128_t>(divisor));
#elif defined(_MSC_VER) && defined(_M_X64)
        unsigned __int64 remainder = 0;
        return static_cast<int64_t>(
            _udiv128(high_, low_, static_cast<unsigned __int64>(divisor), &remainder));
#else
        return static_cast<int64_t>(total_ / static_cast<long double>(divisor));
#endif
    }

private:
#if defined(__SIZEOF_INT128__)
    int128_t total_ = 0;
#elif defined(_MSC_VER) && defined(_M_X64)
    unsigned __int64 high_ = 0;
    unsigned __int64 low_ = 0;
#else
    long double total_ = 0.0L;
#endif
};

} // namespace exchange
