#include <cstdio>
#include <hegel/hegel.h>
#include <stdexcept>

namespace gs = hegel::generators;

static int passing_runs = 0;

HEGEL_TEST(always_fails,
           {.database = hegel::Database::disabled()})(hegel::TestCase& tc) {
    tc.draw(gs::integers<int>());
    throw std::runtime_error("always fails");
}

HEGEL_TEST(fails_with_non_std_exception,
           {.database = hegel::Database::disabled()})(hegel::TestCase& tc) {
    tc.draw(gs::integers<int>());
    throw 42;
}

HEGEL_TEST(always_passes,
           {.test_cases = 5, .database = hegel::Database::disabled()})
(hegel::TestCase& tc) {
    tc.draw(gs::integers<int>());
    passing_runs++;
}

int main() {
    int rc = hegel::run_all_tests();
    if (rc != 1) {
        std::fprintf(stderr, "expected run_all_tests() to return 1, got %d\n",
                     rc);
        return 1;
    }
    if (passing_runs != 5) {
        std::fprintf(stderr, "expected the passing test to run 5 cases, %d\n",
                     passing_runs);
        return 1;
    }
    std::puts("run_all_tests OK");
    return 0;
}
