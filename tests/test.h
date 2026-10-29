#pragma once
// Minimaler Test-Runner: TEST(name) { CHECK(...); CHECK_EQ(a, b); }
#include <cstdio>
#include <functional>
#include <vector>

namespace testing {
struct Case { const char* name; void (*fn)(); };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
struct Registrar { Registrar(const char* n, void (*f)()) { registry().push_back({n, f}); } };
}

#define TEST(name) \
    static void test_##name(); \
    static testing::Registrar reg_##name(#name, test_##name); \
    static void test_##name()

#define CHECK(cond) \
    do { if (!(cond)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++testing::failures(); } } while (0)

#define CHECK_EQ(a, b) \
    do { if (!((a) == (b))) { std::printf("  FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); ++testing::failures(); } } while (0)

