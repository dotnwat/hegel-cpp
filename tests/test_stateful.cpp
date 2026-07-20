#include <gtest/gtest.h>
#include <hegel/hegel.h>

#include <ApprovalTests.hpp>

namespace gs = hegel::generators;

using ApprovalTests::Approvals;

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

namespace {
    struct Stack : hegel::stateful::StateMachine<Stack> {
        std::vector<int> state;
        std::vector<hegel::stateful::Rule<Stack>> rules() {
            return {
                hegel::stateful::Rule<Stack>("push",
                                             [](hegel::TestCase& tc, Stack& m) {
                                                 m.state.push_back(tc.draw(
                                                     gs::integers<int>()));
                                             }),
                hegel::stateful::Rule<Stack>("pop",
                                             [](hegel::TestCase& tc, Stack& m) {
                                                 tc.assume(!m.state.empty());
                                                 m.state.pop_back();
                                             }),
            };
        }
    };

    struct Empty : hegel::stateful::StateMachine<Empty> {
        std::vector<hegel::stateful::Rule<Empty>> rules() { return {}; }
    };

    struct BoundedCounter : hegel::stateful::StateMachine<BoundedCounter> {
        int s = 0;
        std::vector<hegel::stateful::Rule<BoundedCounter>> rules() {
            return {hegel::stateful::Rule<BoundedCounter>(
                "inc", [](hegel::TestCase&, BoundedCounter& m) { m.s += 1; })};
        }
        std::vector<hegel::stateful::Invariant<BoundedCounter>> invariants() {
            return {hegel::stateful::Invariant<BoundedCounter>(
                "bounded", [](const BoundedCounter& m) {
                    if (m.s >= 2) {
                        throw std::runtime_error("bound violated");
                    }
                })};
        }
    };

    struct StoppingCounter : hegel::stateful::StateMachine<StoppingCounter> {
        int s = 0;
        std::vector<hegel::stateful::Rule<StoppingCounter>> rules() {
            return {hegel::stateful::Rule<StoppingCounter>(
                "inc", [](hegel::TestCase&, StoppingCounter& m) {
                    m.s += 1;
                    if (m.s >= 100) {
                        throw std::runtime_error("done");
                    }
                })};
        }
    };

    struct Allocator : hegel::stateful::StateMachine<Allocator> {
        std::set<int> live;
        hegel::stateful::Pool<int> handles;
        int next_handle = 0;

        explicit Allocator(hegel::TestCase& tc) : handles(tc) {}

        std::vector<hegel::stateful::Rule<Allocator>> rules() {
            return {
                hegel::stateful::Rule<Allocator>(
                    "alloc",
                    [](hegel::TestCase&, Allocator& m) {
                        int h = m.next_handle++;
                        m.handles.add(h);
                        m.live.insert(h);
                    }),
                hegel::stateful::Rule<Allocator>(
                    "free",
                    [](hegel::TestCase& tc, Allocator& m) {
                        int h = tc.draw(
                            hegel::stateful::values_consumed(m.handles));
                        m.live.erase(h);
                    }),
            };
        }
        std::vector<hegel::stateful::Invariant<Allocator>> invariants() {
            return {hegel::stateful::Invariant<Allocator>(
                "sizes_agree", [](const Allocator& m) {
                    if (m.handles.size() != m.live.size()) {
                        throw std::runtime_error("pool and live set diverged");
                    }
                })};
        }
    };

    struct Adder : hegel::stateful::StateMachine<Adder> {
        int s = 0;
        std::vector<hegel::stateful::Rule<Adder>> rules() {
            return {hegel::stateful::Rule<Adder>(
                "step", [](hegel::TestCase& tc, Adder& m) {
                    m.s += tc.draw(
                        gs::integers<int>({.min_value = 1, .max_value = 9}));
                })};
        }
    };
} // namespace

TEST(Stateful, BasicRun) {
    hegel::test([](hegel::TestCase& tc) {
        Stack machine;
        hegel::stateful::run(machine, tc);
    });
}

TEST(Stateful, EmptyRulesThrows) {
    EXPECT_THROW(hegel::test([](hegel::TestCase& tc) {
                     Empty machine;
                     hegel::stateful::run(machine, tc);
                 }),
                 std::invalid_argument);
}

TEST(Stateful, InvariantViolationReported) {
    EXPECT_THROW(hegel::test([](hegel::TestCase& tc) {
                     BoundedCounter machine;
                     hegel::stateful::run(machine, tc);
                 }),
                 std::runtime_error);
}

TEST(Stateful, SingleModeRunsUntilRuleStops) {
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         StoppingCounter machine;
                         hegel::stateful::run(machine, tc);
                     },
                     hegel::Settings{.database = hegel::Database::disabled(),
                                     .mode = hegel::Mode::SingleTestCase}),
                 std::runtime_error);
}

TEST(Stateful, PoolAsState) {
    hegel::test([](hegel::TestCase& tc) {
        Allocator machine(tc);
        hegel::stateful::run(machine, tc);
    });
}

TEST(Stateful, TraceNestsDrawsAndHidesStopDecision) {
    testing::internal::CaptureStderr();
    hegel::test(
        [](hegel::TestCase& tc) {
            Adder machine;
            hegel::stateful::run(machine, tc);
        },
        hegel::Settings{.test_cases = 1,
                        .verbosity = hegel::Verbosity::Verbose,
                        .seed = 1,
                        .derandomize = false,
                        .database = hegel::Database::disabled(),
                        .stateful_step_count = 3});
    std::string out = testing::internal::GetCapturedStderr();
    Approvals::verify(out);
}

TEST(Stateful, CounterexamplePrintsRuleDraws) {
    struct Overflowing : hegel::stateful::StateMachine<Overflowing> {
        int s = 0;
        std::vector<hegel::stateful::Rule<Overflowing>> rules() {
            return {hegel::stateful::Rule<Overflowing>(
                "add", [](hegel::TestCase& tc, Overflowing& m) {
                    m.s += tc.draw(
                        gs::integers<int>({.min_value = 1, .max_value = 9}));
                    if (m.s >= 3) {
                        throw std::runtime_error("accumulator overflowed");
                    }
                })};
        }
    };
    testing::internal::CaptureStderr();
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         Overflowing machine;
                         hegel::stateful::run(machine, tc);
                     },
                     hegel::Settings{.database = hegel::Database::disabled(),
                                     .stateful_step_count = 5}),
                 std::runtime_error);
    std::string out = testing::internal::GetCapturedStderr();
    Approvals::verify(out);
}