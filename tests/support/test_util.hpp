#pragma once

#include <cstdio>

namespace test {

inline int failures = 0;

inline int summary() {
    if (failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d TEST(S) FAILED\n", failures);
    return 1;
}

} // namespace test

#define CHECK(condition)                                                                                     \
    do {                                                                                                     \
        if (!(condition)) {                                                                                  \
            std::printf("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #condition);                               \
            ++test::failures;                                                                                \
        }                                                                                                    \
    } while (false)
