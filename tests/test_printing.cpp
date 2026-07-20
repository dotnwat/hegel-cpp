// Printing and failure reporting: the human-facing output Hegel emits — failure
// replays, blobs, multiple-failure reports (driven through the subject
// subprocess), and per-verbosity diagnostics (captured in-process).

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <string>

#include <hegel/hegel.h>
#include <hegel/internal.h>

#include <ApprovalTests.hpp>

#include "common/subprocess.h"
#include "common/utils.h"

using ApprovalTests::Approvals;
using hegel::tests::common::assert_matches_regex;
using hegel::tests::common::run_subject;
using hegel::tests::common::SubprocessResult;

#ifndef HEGEL_SUBJECT_BIN
#error "HEGEL_SUBJECT_BIN must be defined at build time"
#endif

namespace gs = hegel::generators;

// ---------------------------------------------------------------------------
// Failure output, driven through the prebuilt subject binary
// ---------------------------------------------------------------------------

namespace {
    // Run the prebuilt subject binary for one scenario (see subject_main.cpp).
    // No per-test recompile, so these tests run fast and in parallel.
    SubprocessResult run_scenario(const std::string& name) {
        return run_subject(HEGEL_SUBJECT_BIN, {name});
    }
} // namespace

TEST(Output, PrintBlob) {
    SubprocessResult r = run_scenario("print_blob");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(Failure blob:)");
}

TEST(Output, FailingTest) {
    SubprocessResult r = run_scenario("failing");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(auto draw_1 = 0;)");
    assert_matches_regex(r.stderr_data, R"(intentional failure: 0\b)");
}

TEST(Output, OriginStableAcrossDrawnValues) {
    SubprocessResult r = run_scenario("stable_origin");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(auto draw_1 = 10;)");
    assert_matches_regex(r.stderr_data, R"(failure with x=10\b)");
}

// Non-std exceptions carry no message, so only the replay's draw output is
// portable to assert on; the exception itself terminates the subject.
TEST(Output, NonStdExceptionIsHandled) {
    SubprocessResult r = run_scenario("throw_int");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(auto draw_1 = 5;)");
}

TEST(Output, CustomNonStdExceptionIsHandled) {
    SubprocessResult r = run_scenario("throw_custom");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(auto draw_1 = 5;)");
}

TEST(Output, MultipleFailuresReported) {
    SubprocessResult r = run_scenario("multiple_failures");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data,
                         R"(Hegel test failed with 2 distinct failures)");
    assert_matches_regex(r.stderr_data, R"(Failure 1:)");
    assert_matches_regex(r.stderr_data, R"(Failure 2:)");
    assert_matches_regex(r.stderr_data, R"(even bug with x=10\b)");
    assert_matches_regex(r.stderr_data, R"(odd bug with x=11\b)");
}

TEST(Output, MultipleFailuresOffReportsOne) {
    SubprocessResult r = run_scenario("multiple_failures_off");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(\w+ bug with x=1[01]\b)");
    EXPECT_EQ(r.stderr_data.find("distinct failures"), std::string::npos)
        << r.stderr_data;
}

TEST(Output, ExceptionMessageIsShown) {
    SubprocessResult r = run_scenario("exception_message");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(custom exception for x=7)");
}

// ---------------------------------------------------------------------------
// Per-verbosity diagnostics, captured in-process
// ---------------------------------------------------------------------------

namespace {
    constexpr const char* kNote = "SENTINEL_NOTE";

    hegel::Settings with_verbosity(hegel::Verbosity v) {
        return hegel::Settings{.test_cases = 5,
                               .verbosity = v,
                               .database = hegel::Database::disabled()};
    }

    std::string run_capturing_stderr(const hegel::Settings& settings) {
        testing::internal::CaptureStderr();
        hegel::test(
            [](hegel::TestCase& tc) {
                tc.note(kNote);
                (void)tc.draw(gs::integers<int>());
            },
            settings);
        return testing::internal::GetCapturedStderr();
    }

    bool contains(const std::string& haystack, const char* needle) {
        return haystack.find(needle) != std::string::npos;
    }
} // namespace

// Quiet suppresses every per-case diagnostic.
TEST(Diagnostics, QuietSuppressesEverything) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Quiet));
    EXPECT_FALSE(contains(out, kNote));
    EXPECT_FALSE(contains(out, "auto draw_1"));
}

// Normal does not print per-case notes while the property is passing (notes are
// reserved for the final replay of a counterexample, which a passing run has).
TEST(Diagnostics, NormalSuppressesNotesWhilePassing) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Normal));
    EXPECT_FALSE(contains(out, kNote));
}

// Verbose prints notes and drawn values on every case.
TEST(Diagnostics, VerbosePrintsNotesAndValues) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Verbose));
    EXPECT_TRUE(contains(out, kNote));
    EXPECT_TRUE(contains(out, "auto draw_1 = "));
}

// Debug prints everything Verbose does (engine-side shrinker tracing is the
// engine's own output and not asserted on here).
TEST(Diagnostics, DebugPrintsNotesAndValues) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Debug));
    EXPECT_TRUE(contains(out, kNote));
    EXPECT_TRUE(contains(out, "auto draw_1 = "));
}

// A throw that isn't a std::exception exercises the catch(...) fallback, which
// records the exception's type name as the failure origin; the single-failure
// re-raise then surfaces the original exception, not a wrapper.
TEST(Diagnostics, NonStandardExceptionOrigin) {
    EXPECT_THROW(hegel::test([](hegel::TestCase&) { throw 42; },
                             with_verbosity(hegel::Verbosity::Quiet)),
                 int);
}

TEST(Diagnostics, InternalExceptionMessages) {
    EXPECT_STREQ(hegel::internal::HegelReject().what(), "test case rejected");
    EXPECT_STREQ(hegel::internal::HegelStopTest().what(),
                 "test case stopped by backend");
}

// ---------------------------------------------------------------------------
// Draw naming: bare names, 1-based suffixes for repeatable draws, one line
// per user-level draw. Deterministic outputs are snapshotted whole with
// ApprovalTests (tests/approvals/*.approved.txt).
// ---------------------------------------------------------------------------

namespace {
    // Runs one Verbose test case in-process and returns its stderr.
    std::string run_verbose(const std::function<void(hegel::TestCase&)>& body,
                            int64_t test_cases = 1) {
        testing::internal::CaptureStderr();
        hegel::test(body,
                    hegel::Settings{.test_cases = test_cases,
                                    .verbosity = hegel::Verbosity::Verbose,
                                    .database = hegel::Database::disabled()});
        return testing::internal::GetCapturedStderr();
    }
} // namespace

// A user-supplied name drawn once prints bare, with no suffix.
TEST(DrawNames, NamedDrawPrintsBareName) {
    std::string out = run_verbose(
        [](hegel::TestCase& tc) { (void)tc.draw(gs::just(5), "count"); });
    Approvals::verify(out);
}

// Unnamed draws use the base name "draw" with 1-based suffixes, in draw
// order.
TEST(DrawNames, UnnamedDrawsAreNumbered) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        (void)tc.draw(gs::just(1));
        (void)tc.draw(gs::just(2));
    });
    Approvals::verify(out);
}

// A repeatable name is suffixed even for a single use.
TEST(DrawNames, RepeatableSingleUseIsSuffixed) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        (void)tc.draw(gs::just(3), "x", /*repeatable=*/true);
    });
    Approvals::verify(out);
}

TEST(DrawNames, RepeatableDrawsNumberInOrder) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        for (int i = 1; i <= 3; ++i) {
            (void)tc.draw(gs::just(i), "x", /*repeatable=*/true);
        }
    });
    Approvals::verify(out);
}

// Suffix allocation skips display names an earlier draw already took.
TEST(DrawNames, SuffixAllocationSkipsTakenNames) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        (void)tc.draw(gs::just(0), "x_1");
        (void)tc.draw(gs::just(1), "x", /*repeatable=*/true);
        (void)tc.draw(gs::just(2), "x", /*repeatable=*/true);
    });
    Approvals::verify(out);
}

// Reusing a non-repeatable name is a programming error.
TEST(DrawNames, NonRepeatableReuseThrows) {
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         (void)tc.draw(gs::just(1), "x");
                         (void)tc.draw(gs::just(2), "x");
                     },
                     hegel::Settings{.test_cases = 1,
                                     .verbosity = hegel::Verbosity::Quiet,
                                     .database = hegel::Database::disabled()}),
                 std::logic_error);
}

// HEGEL_DRAW declares the variable and prints under its name.
TEST(DrawNames, MacroBindsAndPrints) {
    int seen = 0;
    std::string out = run_verbose([&seen](hegel::TestCase& tc) {
        HEGEL_DRAW(width, tc, gs::just(7));
        seen = width;
    });
    EXPECT_EQ(seen, 7);
    Approvals::verify(out);
}

// A collection draw prints one composed line, not one line per element.
TEST(DrawNames, CollectionDrawPrintsOneComposedLine) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        (void)tc.draw(gs::vectors(gs::just(5), {.min_size = 2, .max_size = 2}));
    });
    Approvals::verify(out);
}

// Draws inside a compose body are internal to the composed generator; only
// the outermost draw prints, with the final composed value.
TEST(DrawNames, ComposeInnerDrawsAreSilent) {
    auto gen = gs::compose([](const hegel::TestCase& tc) {
        int a = tc.draw(gs::just(1));
        int b = tc.draw(gs::just(2));
        return a + b;
    });
    std::string out =
        run_verbose([&gen](hegel::TestCase& tc) { (void)tc.draw(gen); });
    Approvals::verify(out);
}

TEST(DrawNames, CountersResetPerCase) {
    std::string out = run_verbose(
        [](hegel::TestCase& tc) {
            (void)tc.draw(gs::integers<int>().map([](int) { return 3; }), "x",
                          /*repeatable=*/true);
        },
        /*test_cases=*/3);
    Approvals::verify(out);
}
