#pragma once

#include "repr.h"
#include "test_case.h"

#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * @brief Internal utilities exposed in public headers.
 */
namespace hegel::internal {
    /// @cond INTERNAL

    /* Typed draw primitives over libhegel's C ABI. Declared here (rather
     * than in a private header) because the generator templates in the
     * public headers call them from their do_draw implementations. Each
     * throws HegelStopTest when the engine's choice budget is exhausted,
     * HegelReject when the draw rejects itself, and std::runtime_error on
     * any other engine failure.
     */

    // Span labels understood by the engine's shrinker. Values mirror the
    // C ABI's hegel_label_t (static_asserted in src/engine.cpp).
    enum class SpanLabel : uint64_t {
        List = 1,
        ListElement = 2,
        Set = 3,
        SetElement = 4,
        Map = 5,
        MapEntry = 6,
        Tuple = 7,
        OneOf = 8,
        Optional = 9,
        FlatMap = 11,
        Filter = 12,
        Mapped = 13,
        SampledFrom = 14,
        EnumVariant = 15,
        StatefulRule = 1000
    };

    // Open / close a labeled span around a group of draws so the shrinker
    // can treat them as a unit. stop_span(tc, true) marks the span rejected
    // (e.g. a filter predicate failed) so the engine retries from before it.
    void start_span(const TestCase& tc, SpanLabel label);
    void stop_span(const TestCase& tc, bool discard = false);

    int64_t draw_integer(const TestCase& tc, int64_t min_value,
                         int64_t max_value);
    // Uses the engine's big-integer draw when the range exceeds int64_t.
    uint64_t draw_integer_unsigned(const TestCase& tc, uint64_t min_value,
                                   uint64_t max_value);
    bool draw_boolean(const TestCase& tc, double p,
                      std::optional<bool> forced = std::nullopt);
    double draw_float(const TestCase& tc, uint32_t width, double min_value,
                      double max_value, bool allow_nan, bool allow_infinity,
                      bool exclude_min, bool exclude_max,
                      double smallest_nonzero_magnitude);

    // Engine-managed variable-length collections: the engine decides how
    // many elements to produce; the caller loops on collection_more and
    // draws one element per `true`. Pass no_max_size for no upper bound.
    inline constexpr uint64_t no_max_size = ~uint64_t{0};
    int64_t new_collection(const TestCase& tc, uint64_t min_size,
                           uint64_t max_size);
    bool collection_more(const TestCase& tc, int64_t collection_id);
    // Tell the engine the last element produced is unacceptable (e.g. a
    // duplicate in a set) so it tries a different one.
    void collection_reject(const TestCase& tc, int64_t collection_id,
                           const char* why);

    int64_t new_pool(const TestCase& tc);
    int64_t pool_add(const TestCase& tc, int64_t pool_id);
    int64_t draw_variable(const TestCase& tc, int64_t pool_id, bool consume);
    int64_t draw_rule(const TestCase& tc, int64_t state_machine_id);
    int64_t new_state_machine(const TestCase& tc,
                              const std::vector<std::string>& rule_names,
                              const std::vector<std::string>& invariant_names);
    bool is_single_test_case(const TestCase& tc);
    int64_t stateful_step_count(const TestCase& tc);

    class NoteIndentScope {
      public:
        explicit NoteIndentScope(const TestCase& tc);
        ~NoteIndentScope();
        NoteIndentScope(const NoteIndentScope&) = delete;
        NoteIndentScope& operator=(const NoteIndentScope&) = delete;

      private:
        const TestCase& tc_;
    };

    class DrawLogScope {
      public:
        DrawLogScope(const TestCase& tc, std::string_view name,
                     bool repeatable);
        ~DrawLogScope();
        DrawLogScope(const DrawLogScope&) = delete;
        DrawLogScope& operator=(const DrawLogScope&) = delete;

        bool should_log() const;

        void log(const std::string& rendered) const;

      private:
        const TestCase& tc_;
        bool outermost_;
        std::string display_;
    };
    /* Exception thrown when a test case is rejected and should be
     * discarded (e.g. by `TestCase::assume(false)`, an exhausted
     * `filter()`, or an `UnsatisfiedAssumption` from the engine).
     * The runner records the case as INVALID and continues.
     */
    class HegelReject : public std::exception {
      public:
        const char* what() const noexcept override {
            return "test case rejected";
        }
    };

    /* Exception thrown when the backend tells us to abandon the current
     * test iteration entirely (StopTest, Overflow, FlakyStrategyDefinition,
     * FlakyReplay). The runner unwinds the test body and skips
     * `mark_complete` — the engine already knows the iteration is over.
     */
    class HegelStopTest : public std::exception {
      public:
        const char* what() const noexcept override {
            return "test case stopped by backend";
        }
    };

    /* Exception thrown by a failed HEGEL_REQUIRE / HEGEL_REQUIRE_EQUAL. It
     * carries the call site as its origin. The engine groups counterexamples
     * by origin, so an origin taken from the throw site inside the library
     * would make every failed requirement in a run look like one bug.
     */
    class HegelRequireFailure : public std::runtime_error {
      public:
        HegelRequireFailure(std::string origin, const std::string& message)
            : std::runtime_error(message), origin_(std::move(origin)) {}

        const std::string& origin() const noexcept { return origin_; }

      private:
        std::string origin_;
    };

    // Whether notes and drawn values print for this test case. False on a
    // case that only feeds the search. TestCase::note() applies this itself;
    // ask first only to skip building a message that would be dropped.
    bool diagnostics_visible(const TestCase& tc);

    // Renders the difference between two rendered values. Parts only in
    // `left` are marked "-", parts only in `right` "+", and a part that
    // holds the change is descended into rather than printed whole.
    std::string render_node_diff(const ReprNode& left, const ReprNode& right);

    [[noreturn]] void fail_require(const char* origin, std::string message);

    // Implementation of HEGEL_FAIL. `origin` is the macro's call site.
    [[noreturn]] inline void fail_impl(const TestCase& tc, const char* origin,
                                       std::string_view message) {
        // The test case is accepted for symmetry with the other per-test-case
        // operations. The failure itself is client-side.
        (void)tc;
        fail_require(origin, std::string(message));
    }

    // Implementation of HEGEL_REQUIRE. `origin` is the macro's call site.
    inline void require_impl(const TestCase& tc, const char* origin,
                             bool condition, std::string_view message = "") {
        (void)tc;
        if (!condition) {
            fail_require(origin, message.empty()
                                     ? "require: condition was false"
                                     : std::string(message));
        }
    }

    // Implementation of HEGEL_REQUIRE_EQUAL. The two values are compared as
    // repr() renders them, so neither needs an operator==. That rendering is
    // the comparison, so it happens on every call.
    template <typename T>
    void require_equal_impl(const TestCase& tc, const char* origin,
                            const T& left, const T& right,
                            std::string_view message = "") {
        std::string left_repr = repr(left);
        std::string right_repr = repr(right);
        if (left_repr == right_repr) {
            return;
        }
        std::string text = message.empty() ? "require_equal: values differ"
                                           : std::string(message);
        // note() drops a message that will not print, but only after its
        // argument is built. This guard keeps the two value trees and the
        // descent through them out of every case that fails on the way to
        // the counterexample, which is most of them.
        if (diagnostics_visible(tc)) {
            tc.note(text + " (- lhs / + rhs):\n" +
                    render_node_diff(repr_node(left), repr_node(right)));
        }
        fail_require(origin, std::move(text));
    }

} // namespace hegel::internal

/// @endcond
