// HEGEL_REQUIRE and HEGEL_REQUIRE_EQUAL: the failure messages, the value diff
// the report shows for unequal values, and the call-site origins that keep
// distinct requirements from collapsing into one reported failure.

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <map>
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
                std::regex(R"("[A-Za-z0-9+/=]{8,}")"), R"("[blob]")"));
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

// Two requirements on different source lines are two distinct bugs, so the
// report holds two numbered sections. The origin comes from the macro's call
// site: an origin taken from the throw site inside the library would report
// both as the same failure.
TEST(Require, DistinctCallSitesAreDistinctFailures) {
    std::string out = capture(
        [](hegel::TestCase& tc) {
            HEGEL_DRAW(
                tc, x,
                gs::integers<int32_t>({.min_value = 0, .max_value = 100}));
            HEGEL_REQUIRE(tc, x < 60, "too big");
            HEGEL_REQUIRE(tc, x > 30, "too small");
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    Approvals::verify(out, scrub_blob());
}

// One requirement failing on many different values is one bug, so the report
// stays a single unnumbered section.
TEST(Require, OneCallSiteIsOneFailure) {
    std::string out = capture(
        [](hegel::TestCase& tc) {
            HEGEL_DRAW(
                tc, x,
                gs::integers<int32_t>({.min_value = 0, .max_value = 100}));
            HEGEL_REQUIRE(tc, x < 60, "too big");
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    Approvals::verify(out, scrub_blob());
}

TEST(Fail, DefaultMessage) {
    std::string out =
        capture([](hegel::TestCase& tc) { HEGEL_FAIL(tc); }); // NOLINT
    Approvals::verify(out, scrub_blob());
}

TEST(Fail, CarriesItsMessage) {
    std::string out = capture(
        [](hegel::TestCase& tc) { HEGEL_FAIL(tc, "the invariant broke"); });
    Approvals::verify(out, scrub_blob());
}

// Like HEGEL_REQUIRE, each use carries its own source position, so two
// failures on different lines stay two bugs.
TEST(Fail, DistinctCallSitesAreDistinctFailures) {
    std::string out = capture(
        [](hegel::TestCase& tc) {
            HEGEL_DRAW(
                tc, x,
                gs::integers<int32_t>({.min_value = 0, .max_value = 100}));
            if (x >= 60) {
                HEGEL_FAIL(tc, "too big");
            }
            if (x <= 30) {
                HEGEL_FAIL(tc, "too small");
            }
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    Approvals::verify(out, scrub_blob());
}

// Two different exception types are two origins, which is the only way a raw
// throw can produce distinct failures today.
TEST(Fail, RawThrowsOfDifferentTypesStayDistinct) {
    std::string out = capture(
        [](hegel::TestCase& tc) {
            HEGEL_DRAW(
                tc, x,
                gs::integers<int32_t>({.min_value = 0, .max_value = 100}));
            if (x >= 60) {
                throw std::runtime_error("too big");
            }
            if (x <= 30) {
                throw std::logic_error("too small");
            }
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    EXPECT_NE(out.find("Failure 1 of 2:"), std::string::npos) << out;
    Approvals::verify(out, scrub_blob());
}

// HEGEL_FAIL does not return, so a branch that ends in one needs no value.
TEST(Fail, DoesNotReturn) {
    int seen = -1;
    hegel::test(
        [&seen](hegel::TestCase& tc) {
            auto positive_or_fail = [&tc](int n) -> int {
                if (n < 0) {
                    HEGEL_FAIL(tc, "negative");
                }
                return n;
            };
            seen = positive_or_fail(7);
        },
        hegel::Settings{.test_cases = 1,
                        .database = hegel::Database::disabled()});
    EXPECT_EQ(seen, 7);
}

// Values with no inner structure diff as a whole.
TEST(RequireEqual, ScalarsDiffAsWholeValues) {
    std::string out =
        capture([](hegel::TestCase& tc) { HEGEL_REQUIRE_EQUAL(tc, 3, 4); });
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

// A map diffs by entry, taking its entries from the map itself.
TEST(RequireEqual, MapsDiffPerEntry) {
    std::string out = capture([](hegel::TestCase& tc) {
        std::map<std::string, int> lhs{{"a", 1}, {"b", 2}};
        std::map<std::string, int> rhs{{"a", 1}, {"b", 9}};
        HEGEL_REQUIRE_EQUAL(tc, lhs, rhs);
    });
    Approvals::verify(out, scrub_blob());
}

// These sit at file scope because repr() reads a type's name out of
// __PRETTY_FUNCTION__, and a type declared inside a function is named
// differently by GCC and by Clang.
struct Point {
    int x;
    int y;
};

struct Team {
    std::string name;
    std::vector<int> scores;
};

// A struct diffs by field.
TEST(RequireEqual, StructsDiffPerField) {
    std::string out = capture([](hegel::TestCase& tc) {
        Point lhs{1, 2};
        Point rhs{1, 9};
        HEGEL_REQUIRE_EQUAL(tc, lhs, rhs);
    });
    Approvals::verify(out, scrub_blob());
}

// A value holding a string keeps commas and braces inside the string out of
// the element split.
TEST(RequireEqual, StringsHoldingSeparatorsStayWhole) {
    std::string out = capture([](hegel::TestCase& tc) {
        std::vector<std::string> lhs{"a, b", "c{d"};
        std::vector<std::string> rhs{"a, b", "c}d"};
        HEGEL_REQUIRE_EQUAL(tc, lhs, rhs);
    });
    Approvals::verify(out, scrub_blob());
}

// A difference inside a nested value is followed down to the part that
// changed, instead of marking the whole element holding it.
TEST(RequireEqual, NestedValuesDescendToTheChangedPart) {
    std::string out = capture([](hegel::TestCase& tc) {
        std::vector<std::vector<int>> lhs{{1, 2}, {3, 4}};
        std::vector<std::vector<int>> rhs{{1, 2}, {3, 9}};
        HEGEL_REQUIRE_EQUAL(tc, lhs, rhs);
    });
    Approvals::verify(out, scrub_blob());
}

// The descent has no depth limit.
TEST(RequireEqual, DeeplyNestedValuesDescendAllTheWay) {
    std::string out = capture([](hegel::TestCase& tc) {
        std::vector<std::vector<std::vector<int>>> lhs{{{1, 2}}, {{3, 4}}};
        std::vector<std::vector<std::vector<int>>> rhs{{{1, 2}}, {{3, 9}}};
        HEGEL_REQUIRE_EQUAL(tc, lhs, rhs);
    });
    Approvals::verify(out, scrub_blob());
}

// A field naming a composite keeps its name on the line that opens the
// descent.
TEST(RequireEqual, StructFieldsDescendUnderTheirName) {
    std::string out = capture([](hegel::TestCase& tc) {
        Team lhs{"a", {1, 2, 3}};
        Team rhs{"a", {1, 9, 3}};
        HEGEL_REQUIRE_EQUAL(tc, lhs, rhs);
    });
    Approvals::verify(out, scrub_blob());
}

// The macro passes its arguments along untouched, so a value written with a
// comma in its template arguments or its braces needs no parenthesizing and
// no variable to hold it. A macro that stringified an argument could not do
// this. Prevents regressions from future macro changes.
TEST(RequireEqual, ValuesHoldingCommasNeedNoVariable) {
    std::string out = capture([](hegel::TestCase& tc) {
        std::pair<int, int> p{1, 2};
        HEGEL_REQUIRE_EQUAL(tc, std::pair<int, int>{1, 9}, p);
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

// The diff only feeds the failure report, so the report shows it once, for
// the final replay, and not for each of the cases that led there.
TEST(RequireEqual, DiffIsRenderedOnceForTheReport) {
    std::string out = capture([](hegel::TestCase& tc) {
        HEGEL_DRAW(tc, x,
                   gs::integers<int32_t>({.min_value = 0, .max_value = 100}));
        HEGEL_REQUIRE_EQUAL(tc, x, 101);
    });
    Approvals::verify(out, scrub_blob());
}

// Quiet suppresses the report, so no diff reaches stderr at all.
TEST(RequireEqual, QuietRendersNoDiff) {
    std::string out =
        capture([](hegel::TestCase& tc) { HEGEL_REQUIRE_EQUAL(tc, 3, 4); },
                hegel::Settings{.verbosity = hegel::Verbosity::Quiet});
    Approvals::verify(out);
}
