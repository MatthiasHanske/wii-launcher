#include "test.h"

TEST(sanity) { CHECK_EQ(1 + 1, 2); }

int main() {
    int passed = 0, failed = 0;
    for (const auto& c : testing::registry()) {
        int before = testing::failures();
        c.fn();
        if (testing::failures() == before) ++passed;
        else { ++failed; std::printf("  in test: %s\n", c.name); }
    }
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
