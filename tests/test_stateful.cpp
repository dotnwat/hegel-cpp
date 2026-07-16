#include <gtest/gtest.h>
#include <hegel/hegel.h>

namespace gs = hegel::generators;

TEST(Pools, PoolsRoundTrip) {
    hegel::test([](hegel::TestCase& tc) {
        hegel::stateful::Pool<int> pool = hegel::stateful::Pool<int>(tc);
        std::set<int> original_set =
            tc.draw(gs::sets(gs::integers<int>(), {.max_size = 10}));

        for (int num : original_set) {
            pool.add(num);
        }

        std::set<int> returned_set;
        for (int i = 0; i < original_set.size(); i++) {
            returned_set.insert(
                tc.draw(hegel::stateful::values_consumed(pool)));
        }

        assert(original_set == returned_set);
    });
}

TEST(Pools, PoolsNoConsume) {
    hegel::test([](hegel::TestCase& tc) {
        hegel::stateful::Pool<int> pool = hegel::stateful::Pool<int>(tc);
        uint8_t sz = tc.draw(gs::integers<uint8_t>());
        std::set<int> original_set =
            tc.draw(gs::sets(gs::integers<int>(), {.max_size = sz}));

        for (int num : original_set) {
            pool.add(num);
        }

        for (int i = 0; i < original_set.size(); i++) {
            tc.draw(hegel::stateful::values_reusable(pool));
        }

        assert(pool.size() == original_set.size());
    });
}

TEST(Pools, DrawFromEmptyPool) {
    hegel::test([](hegel::TestCase& tc) {
        hegel::stateful::Pool<int> pool = hegel::stateful::Pool<int>(tc);
        tc.draw(hegel::stateful::values_reusable(pool));
        // should not error just a test case rejection
    });
}