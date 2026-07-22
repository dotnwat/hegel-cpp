#pragma once

/**
 * @cond INTERNAL
 */

#include <hegel.h>
#include <hegel/settings.h>

#include <cstddef>
#include <string>

namespace hegel::impl::test_case {

    // Per-case runtime state.
    // Generators reach `tc` through TestCase::data() to drive the typed draw
    // primitives. (The error-reporting context is per-thread.
    struct TestCaseData {
        hegel_test_case_t* tc;
        bool is_final;
        Verbosity verbosity;
        int64_t stateful_step_count;
        Mode mode;
        int note_indent = 0;

        // Releases the owned libhegel handle. The owning TestCase's
        // unique_ptr runs this on scope exit.
        ~TestCaseData();

        // leading whitespace for a printed note/drawn value at the current
        // nesting depth.
        std::string indent_prefix() const {
            return std::string(static_cast<std::size_t>(note_indent) * 2, ' ');
        }

        // Whether per-case diagnostics (notes, drawn values) should print:
        // never under Quiet, only on the final replay under Normal, and on
        // every case under Verbose / Debug.
        bool should_log() const {
            switch (verbosity) {
            case Verbosity::Quiet:
                return false;
            case Verbosity::Normal:
                return is_final;
            case Verbosity::Verbose:
            case Verbosity::Debug:
                return true;
            }
            return false; // GCOVR_EXCL_LINE - switch above is exhaustive
        }
    };

} // namespace hegel::impl::test_case

/// @endcond
