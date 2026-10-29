#include "test.h"

#include <string>

#include "core/lru.h"

TEST(lru_empty) {
    LruPolicy<std::string> p;
    CHECK_EQ(p.size(), (size_t)0);
    CHECK_EQ(p.totalBytes(), (size_t)0);
    CHECK(!p.contains("a"));
}

TEST(lru_insert_counts_bytes) {
    LruPolicy<std::string> p;
    p.insert("a", 100);
    p.insert("b", 50);
    CHECK_EQ(p.size(), (size_t)2);
    CHECK_EQ(p.totalBytes(), (size_t)150);
    CHECK(p.contains("a"));
    CHECK(p.contains("b"));
}

TEST(lru_evict_returns_oldest) {
    LruPolicy<std::string> p;
    p.insert("a", 10);
    p.insert("b", 20);
    p.insert("c", 30);
    p.insert("d", 40);
    CHECK_EQ(p.evict(), "a");
    CHECK(!p.contains("a"));
    CHECK_EQ(p.size(), (size_t)3);
    CHECK_EQ(p.totalBytes(), (size_t)90);
    CHECK_EQ(p.evict(), "b");
    CHECK_EQ(p.totalBytes(), (size_t)70);
}

// Vier Keys bei Budget 3 Einträge: Aufrufer räumt, solange zu viel drin ist.
TEST(lru_insert_over_limit_evicts_oldest) {
    LruPolicy<std::string> p;
    const size_t limit = 3 * 100;
    const char* keys[] = {"k1", "k2", "k3", "k4"};
    std::string evicted;
    for (const char* k : keys) {
        p.insert(k, 100);
        while (p.totalBytes() > limit) evicted += p.evict();
    }
    CHECK_EQ(evicted, "k1");
    CHECK_EQ(p.size(), (size_t)3);
    CHECK_EQ(p.totalBytes(), (size_t)300);
    CHECK(!p.contains("k1"));
    CHECK(p.contains("k4"));
}

TEST(lru_touch_rejuvenates) {
    LruPolicy<std::string> p;
    p.insert("a", 1);
    p.insert("b", 1);
    p.insert("c", 1);
    p.touch("a");
    CHECK_EQ(p.evict(), "b");
    CHECK_EQ(p.evict(), "c");
    CHECK_EQ(p.evict(), "a");
    CHECK_EQ(p.size(), (size_t)0);
    CHECK_EQ(p.totalBytes(), (size_t)0);
}

TEST(lru_touch_unknown_is_noop) {
    LruPolicy<std::string> p;
    p.insert("a", 5);
    p.touch("zzz");
    CHECK_EQ(p.size(), (size_t)1);
    CHECK_EQ(p.totalBytes(), (size_t)5);
}

TEST(lru_reinsert_replaces_bytes_and_rejuvenates) {
    LruPolicy<std::string> p;
    p.insert("a", 10);
    p.insert("b", 10);
    p.insert("a", 30);
    CHECK_EQ(p.size(), (size_t)2);
    CHECK_EQ(p.totalBytes(), (size_t)40);
    CHECK_EQ(p.evict(), "b");
}

TEST(lru_erase) {
    LruPolicy<std::string> p;
    p.insert("a", 10);
    p.insert("b", 20);
    CHECK(p.erase("a"));
    CHECK(!p.erase("a"));
    CHECK_EQ(p.size(), (size_t)1);
    CHECK_EQ(p.totalBytes(), (size_t)20);
}

TEST(lru_evict_empty_returns_default) {
    LruPolicy<std::string> p;
    CHECK_EQ(p.evict(), "");
    CHECK_EQ(p.size(), (size_t)0);
}
