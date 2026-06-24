#pragma once

/**
 * @cond INTERNAL
 */

#include <hegel.h>
#include <hegel/settings.h>

namespace hegel::impl::test_case {

    // Per-iteration runtime state. `ctx` and `tc` are borrowed libhegel
    // handles owned by the run loop (src/hegel.cpp); generators reach them
    // through TestCase::data() to drive `hegel_generate`.
    struct TestCaseData {
        hegel_context_t* ctx;
        hegel_test_case_t* tc;
        bool is_last_run;
        Verbosity verbosity;
    };

} // namespace hegel::impl::test_case

/// @endcond
