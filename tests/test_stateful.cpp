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

TEST(Stateful, BasicRun) {
    hegel::test([](hegel::TestCase& tc) {
        auto push_rule = hegel::stateful::Rule<std::vector<int>>(
            "push", [](hegel::TestCase& tc, std::vector<int>& state) {
                int n = tc.draw(gs::integers<int>());
                state.push_back(n);
            });
        auto pop_rule = hegel::stateful::Rule<std::vector<int>>(
            "pop", [](hegel::TestCase& tc, std::vector<int>& state) {
                tc.assume(!state.empty());
                state.pop_back();
            });
        std::vector<int> state;
        hegel::stateful::run(tc, state, {push_rule, pop_rule}, {});
    });
}

TEST(Stateful, EmptyRulesThrows) {
    EXPECT_THROW(hegel::test([](hegel::TestCase& tc) {
                     int state = 0;
                     hegel::stateful::run(
                         tc, state, std::vector<hegel::stateful::Rule<int>>{},
                         {});
                 }),
                 std::invalid_argument);
}

TEST(Stateful, InvariantViolationReported) {
    EXPECT_THROW(hegel::test([](hegel::TestCase& tc) {
                     auto inc = hegel::stateful::Rule<int>(
                         "inc", [](hegel::TestCase&, int& s) { s += 1; });
                     auto bounded = hegel::stateful::Invariant<int>(
                         "bounded", [](const int& s) {
                             if (s >= 2) {
                                 throw std::runtime_error("bound violated");
                             }
                         });
                     int state = 0;
                     hegel::stateful::run(tc, state, {inc}, {bounded});
                 }),
                 std::runtime_error);
}

TEST(Stateful, SingleModeRunsUntilRuleStops) {
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         auto inc = hegel::stateful::Rule<int>(
                             "inc", [](hegel::TestCase&, int& s) {
                                 s += 1;
                                 if (s >= 100) {
                                     throw std::runtime_error("done");
                                 }
                             });
                         int state = 0;
                         hegel::stateful::run(tc, state, {inc}, {});
                     },
                     hegel::Settings{.database = hegel::Database::disabled(),
                                     .mode = hegel::Mode::SingleTestCase}),
                 std::runtime_error);
}

TEST(Stateful, PoolAsState) {
    hegel::test([](hegel::TestCase& tc) {
        struct State {
            std::set<int> live;
            hegel::stateful::Pool<int> handles;
        };
        State state{{}, hegel::stateful::Pool<int>(tc)};
        int next_handle = 0;

        auto alloc = hegel::stateful::Rule<State>(
            "alloc", [&next_handle](hegel::TestCase&, State& s) {
                int h = next_handle++;
                s.handles.add(h);
                s.live.insert(h);
            });
        auto free = hegel::stateful::Rule<State>(
            "free", [](hegel::TestCase& tc, State& s) {
                int h = tc.draw(hegel::stateful::values_consumed(s.handles));
                s.live.erase(h);
            });

        auto sizes_agree = hegel::stateful::Invariant<State>(
            "sizes_agree", [](const State& s) {
                if (s.handles.size() != s.live.size()) {
                    throw std::runtime_error("pool and live set diverged");
                }
            });

        hegel::stateful::run(tc, state, {alloc, free}, {sizes_agree});
    });
}

TEST(Stateful, TraceNestsDrawsAndHidesStopDecision) {
    testing::internal::CaptureStderr();
    hegel::test(
        [](hegel::TestCase& tc) {
            auto step = hegel::stateful::Rule<int>(
                "step", [](hegel::TestCase& tc, int& s) {
                    s += tc.draw(
                        gs::integers<int>({.min_value = 1, .max_value = 9}));
                });
            int state = 0;
            hegel::stateful::run(tc, state, {step}, {});
        },
        hegel::Settings{.test_cases = 1,
                        .verbosity = hegel::Verbosity::Verbose,
                        .database = hegel::Database::disabled(),
                        .stateful_step_count = 3});
    std::string out = testing::internal::GetCapturedStderr();

    // a step's draw is nested two spaces under its header.
    EXPECT_NE(out.find("  Generated:"), std::string::npos) << out;
    // no drawn value is logged at column 0.
    EXPECT_EQ(out.find("\nGenerated:"), std::string::npos) << out;
    // the stop-decision boolean is not printed.
    EXPECT_EQ(out.find("Generated: true"), std::string::npos) << out;
    EXPECT_EQ(out.find("Generated: false"), std::string::npos) << out;
}