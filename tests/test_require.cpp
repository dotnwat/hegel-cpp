// HEGEL_REQUIRE and HEGEL_REQUIRE_EQUAL: the failure messages, the value diff
// the report shows for unequal values, and the call-site origins that keep
// distinct requirements from collapsing into one reported failure.

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <regex>
#include <string>
#include <vector>

#include <hegel/hegel.h>

#include <ApprovalTests.hpp>

using ApprovalTests::Approvals;

namespace gs = hegel::generators;

namespace {
    ApprovalTests::Options scrub_blob() {
        return ApprovalTests::Options(
            ApprovalTests::Scrubbers::createRegexScrubber(
                std::regex(R"RX((rerun with: .*)"[A-Za-z0-9+/=]*")RX"),
                R"RX($1"[blob]")RX"));
    }

    // Runs a property with a fixed seed and returns the report it printed,
    // plus the exception the run re-raised.
    std::string capture(const std::function<void(hegel::TestCase&)>& body,
                        hegel::Settings settings = {}) {
        settings.seed = 1;
        settings.derandomize = false;
        settings.database = hegel::Database::disabled();
        testing::internal::CaptureStderr();
        std::string rethrown = "<no exception>";
        try {
            hegel::test(body, settings);
        } catch (const std::exception& e) {
            rethrown = e.what();
        }
        return testing::internal::GetCapturedStderr() +
               "--- rethrown: " + rethrown + "\n";
    }
} // namespace

TEST(Require, DefaultMessage) {
    std::string out = capture(
        [](hegel::TestCase& tc) { HEGEL_REQUIRE(tc, 1 == 2); }); // NOLINT
    Approvals::verify(out, scrub_blob());
}

TEST(Require, CustomMessage) {
    std::string out = capture([](hegel::TestCase& tc) {
        HEGEL_REQUIRE(tc, false, "the invariant broke");
    });
    Approvals::verify(out, scrub_blob());
}

TEST(Require, SatisfiedRequirementRunsOn) {
    bool reached = false;
    hegel::test(
        [&reached](hegel::TestCase& tc) {
            HEGEL_REQUIRE(tc, true);
            reached = true;
        },
        hegel::Settings{.test_cases = 5,
                        .database = hegel::Database::disabled()});
    EXPECT_TRUE(reached);
}

// Two requirements on different source lines are two distinct bugs. The
// origin comes from the macro's call site: an origin taken from the throw
// site inside the library would report both as the same failure.
TEST(Require, DistinctCallSitesAreDistinctFailures) {
    std::string out = capture(
        [](hegel::TestCase& tc) {
            HEGEL_DRAW(tc, x,
                       gs::integers<int32_t>({.min_value = 0,
                                              .max_value = 100}));
            HEGEL_REQUIRE(tc, x < 60, "too big");
            HEGEL_REQUIRE(tc, x > 30, "too small");
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    EXPECT_NE(out.find("Failure 1 of 2:"), std::string::npos) << out;
    EXPECT_NE(out.find("Failure 2 of 2:"), std::string::npos) << out;
    EXPECT_NE(out.find("too big"), std::string::npos) << out;
    EXPECT_NE(out.find("too small"), std::string::npos) << out;
}

// One requirement failing on many different values is one bug, so the report
// stays a single section.
TEST(Require, OneCallSiteIsOneFailure) {
    std::string out = capture(
        [](hegel::TestCase& tc) {
            HEGEL_DRAW(tc, x,
                       gs::integers<int32_t>({.min_value = 0,
                                              .max_value = 100}));
            HEGEL_REQUIRE(tc, x < 60, "too big");
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    EXPECT_EQ(out.find("Failure 1 of"), std::string::npos) << out;
}

// Values with no inner structure diff as a whole.
TEST(RequireEqual, ScalarsDiffAsWholeValues) {
    std::string out = capture(
        [](hegel::TestCase& tc) { HEGEL_REQUIRE_EQUAL(tc, 3, 4); });
    Approvals::verify(out, scrub_blob());
}

// A composite value diffs element by element, so the report points at the one
// element that differs instead of at the whole value.
TEST(RequireEqual, CompositeValuesDiffPerElement) {
    std::string out = capture([](hegel::TestCase& tc) {
        std::vector<int> lhs{1, 2, 3};
        std::vector<int> rhs{1, 9, 3};
        HEGEL_REQUIRE_EQUAL(tc, lhs, rhs);
    });
    Approvals::verify(out, scrub_blob());
}

// An element only on one side is marked, and the elements around it stay
// common.
TEST(RequireEqual, InsertedElementIsMarked) {
    std::string out = capture([](hegel::TestCase& tc) {
        std::vector<int> lhs{1, 3};
        std::vector<int> rhs{1, 2, 3};
        HEGEL_REQUIRE_EQUAL(tc, lhs, rhs);
    });
    Approvals::verify(out, scrub_blob());
}

TEST(RequireEqual, CustomMessage) {
    std::string out = capture([](hegel::TestCase& tc) {
        HEGEL_REQUIRE_EQUAL(tc, 3, 4, "addition does not commute");
    });
    Approvals::verify(out, scrub_blob());
}

TEST(RequireEqual, EqualValuesRunOn) {
    bool reached = false;
    hegel::test(
        [&reached](hegel::TestCase& tc) {
            std::vector<int> v{1, 2, 3};
            HEGEL_REQUIRE_EQUAL(tc, v, v);
            reached = true;
        },
        hegel::Settings{.test_cases = 5,
                        .database = hegel::Database::disabled()});
    EXPECT_TRUE(reached);
}

// The diff only feeds the failure report, so it is rendered on the final
// replay alone and not on each of the cases that led there.
TEST(RequireEqual, DiffIsRenderedOnceForTheReport) {
    std::string out = capture([](hegel::TestCase& tc) {
        HEGEL_DRAW(tc, x,
                   gs::integers<int32_t>({.min_value = 0, .max_value = 100}));
        HEGEL_REQUIRE_EQUAL(tc, x, 101);
    });
    size_t count = 0;
    for (size_t at = out.find("(- lhs / + rhs)"); at != std::string::npos;
         at = out.find("(- lhs / + rhs)", at + 1)) {
        count++;
    }
    EXPECT_EQ(count, 1u) << out;
}

// Quiet suppresses the report, so no diff reaches stderr at all.
TEST(RequireEqual, QuietRendersNoDiff) {
    std::string out = capture(
        [](hegel::TestCase& tc) { HEGEL_REQUIRE_EQUAL(tc, 3, 4); },
        hegel::Settings{.verbosity = hegel::Verbosity::Quiet});
    EXPECT_EQ(out.find("(- lhs / + rhs)"), std::string::npos) << out;
}
