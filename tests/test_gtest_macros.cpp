// GoogleTest assertion macros inside a Hegel property: what a failed
// assertion tells Hegel, and how many bugs a run reports for assertions that
// fail on different lines.

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

namespace {

    struct RunResult {
        std::string report;   // what the run printed
        std::string rethrown; // the exception the run raised
        int failures;         // the GoogleTest failures the body recorded
    };

    // Runs `body` as a property and holds back the GoogleTest failures its
    // assertions record, which would otherwise fail the enclosing test.
    RunResult run(const std::function<void(hegel::TestCase&)>& body,
                  hegel::Settings settings = {}) {
        settings.seed = 1;
        settings.derandomize = false;
        settings.database = hegel::Database::disabled();

        testing::TestPartResultArray recorded;
        RunResult out{"", "<no exception>", 0};
        {
            testing::ScopedFakeTestPartResultReporter reporter(
                testing::ScopedFakeTestPartResultReporter::
                    INTERCEPT_ONLY_CURRENT_THREAD,
                &recorded);
            testing::internal::CaptureStderr();
            try {
                hegel::test(body, settings);
            } catch (const std::exception& e) {
                out.rethrown = e.what();
            }
            out.report = testing::internal::GetCapturedStderr();
        }
        out.failures = recorded.size();
        return out;
    }

    // Sets gtest_throw_on_failure for as long as it is alive. With the flag
    // on, a failed GoogleTest assertion raises GoogleTestFailureException,
    // which Hegel catches as a failing test case.
    class ThrowOnFailure {
      public:
        ThrowOnFailure() : previous_(GTEST_FLAG_GET(throw_on_failure)) {
            GTEST_FLAG_SET(throw_on_failure, true);
        }
        ~ThrowOnFailure() { GTEST_FLAG_SET(throw_on_failure, previous_); }

        ThrowOnFailure(const ThrowOnFailure&) = delete;
        ThrowOnFailure& operator=(const ThrowOnFailure&) = delete;

      private:
        bool previous_;
    };

    bool contains(const std::string& text, const std::string& part) {
        return text.find(part) != std::string::npos;
    }

    auto small_int() {
        return gs::integers<int32_t>({.min_value = 0, .max_value = 100});
    }

} // namespace

// A GoogleTest assertion compiles and runs inside a property body, and its
// failure reaches the enclosing GoogleTest test.
TEST(GTestMacros, FailedExpectIsRecorded) {
    RunResult out = run(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(small_int());
            EXPECT_LE(x, 50) << "x = " << x;
        },
        hegel::Settings{.test_cases = 50});
    EXPECT_GT(out.failures, 0);
}

// EXPECT_* records its failure and returns, so the body returns normally and
// Hegel counts the case as a pass: no report, no shrinking, and nothing
// raised. The failure is visible only as a GoogleTest failure.
TEST(GTestMacros, FailedExpectDoesNotFailTheProperty) {
    RunResult out = run(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(small_int());
            EXPECT_LE(x, 50);
        },
        hegel::Settings{.test_cases = 50});
    EXPECT_EQ(out.rethrown, "<no exception>");
    EXPECT_FALSE(contains(out.report, "Falsified")) << out.report;
    // One failure per case that broke the property, none of them shrunk.
    EXPECT_GT(out.failures, 1);
}

// ASSERT_* ends the body it fails in, and a property body is a callable, so
// the rest of that body is skipped. Hegel still counts the case as a pass.
TEST(GTestMacros, FailedAssertReturnsFromTheBody) {
    bool reached_end = false;
    RunResult out = run(
        [&reached_end](hegel::TestCase& tc) {
            int32_t x = tc.draw(small_int());
            ASSERT_LT(x, 0); // never true: the generator starts at 0
            reached_end = true;
        },
        hegel::Settings{.test_cases = 50});
    EXPECT_GT(out.failures, 0);
    EXPECT_EQ(out.rethrown, "<no exception>");
    EXPECT_FALSE(reached_end);
}

// With gtest_throw_on_failure on, a failed assertion raises an exception,
// which Hegel treats as a failing case: it shrinks to the smallest value that
// breaks the property and reports it.
TEST(GTestMacros, ThrowOnFailureLetsHegelShrink) {
    ThrowOnFailure throwing;
    RunResult out = run(
        [](hegel::TestCase& tc) {
            HEGEL_DRAW(tc, x, small_int());
            ASSERT_LE(x, 50);
        },
        hegel::Settings{.test_cases = 200});
    EXPECT_NE(out.rethrown, "<no exception>");
    EXPECT_TRUE(contains(out.report, "Falsified after")) << out.report;
    EXPECT_TRUE(contains(out.report, "auto x = 51;")) << out.report;
}

// Two GoogleTest assertions on different lines are one bug, not two.
//
// Hegel groups counterexamples by origin, and the origin of a raised
// exception is its type name. Every GoogleTest assertion raises the same
// type, so both call sites share one origin and the run reports one failure
// even with report_multiple_failures on. The second bug is never reported.
// To keep two properties apart, throw a distinct exception type from each.
TEST(GTestMacros, AssertionsOnDifferentLinesAreOneOrigin) {
    ThrowOnFailure throwing;
    RunResult out = run(
        [](hegel::TestCase& tc) {
            HEGEL_DRAW(tc, x, small_int());
            ASSERT_EQ(x % 2, 0) << "odd";
            ASSERT_EQ(x % 5, 0) << "not a multiple of five";
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    EXPECT_FALSE(contains(out.report, "Failure 1 of 2")) << out.report;
    EXPECT_TRUE(
        contains(out.report, "testing::internal::GoogleTestFailureException"))
        << out.report;
}

// The same property written with two exception types reports both bugs.
TEST(GTestMacros, DistinctExceptionTypesKeepBugsApart) {
    struct Odd : std::runtime_error {
        Odd() : std::runtime_error("odd") {}
    };
    struct NotAMultipleOfFive : std::runtime_error {
        NotAMultipleOfFive() : std::runtime_error("not a multiple of five") {}
    };
    RunResult out = run(
        [](hegel::TestCase& tc) {
            HEGEL_DRAW(tc, x, small_int());
            if (x % 2 != 0) {
                throw Odd();
            }
            if (x % 5 != 0) {
                throw NotAMultipleOfFive();
            }
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    EXPECT_TRUE(contains(out.report, "Failure 1 of 2")) << out.report;
}

// One assertion that fails on many values stays one bug, which is what the
// grouping is for.
TEST(GTestMacros, OneAssertionIsOneFailure) {
    ThrowOnFailure throwing;
    RunResult out = run(
        [](hegel::TestCase& tc) {
            HEGEL_DRAW(tc, x, small_int());
            ASSERT_LT(x, 60) << "too big";
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    EXPECT_FALSE(contains(out.report, "Failure 1 of")) << out.report;
}

// Shrinking re-runs the body many times, so every failing attempt records
// another GoogleTest failure. A property that reports one bug still fills the
// enclosing test with failures.
TEST(GTestMacros, EveryFailedAttemptRecordsAFailure) {
    ThrowOnFailure throwing;
    RunResult out = run(
        [](hegel::TestCase& tc) {
            HEGEL_DRAW(tc, x, small_int());
            ASSERT_LT(x, 60);
        },
        hegel::Settings{.test_cases = 300});
    EXPECT_GT(out.failures, 1);
}

// A property that holds records nothing and raises nothing.
TEST(GTestMacros, PassingAssertionsRunOn) {
    ThrowOnFailure throwing;
    RunResult out = run(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(small_int());
            ASSERT_GE(x, 0);
            ASSERT_LE(x, 100);
        },
        hegel::Settings{.test_cases = 50});
    EXPECT_EQ(out.failures, 0);
    EXPECT_EQ(out.rethrown, "<no exception>");
}
