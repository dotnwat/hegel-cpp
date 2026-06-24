/*
 * hegel.cpp - The hegel::test() entry point.
 *
 * Drives Hegel's native engine (libhegel) in-process through its C ABI:
 * start a run, pull test cases, run the user body, mark each complete, then
 * inspect the aggregate result and replay any counterexamples.
 */

#include <hegel/hegel.h>
#include <hegel/internal.h>
#include <hegel/settings.h>
#include <hegel/test_case.h>

#include <engine.h>
#include <protocol.h>
#include <test_case.h>

#include <hegel.h>

#include <cstddef>
#include <cstdint>
#include <cxxabi.h>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <typeinfo>

namespace hegel {

    namespace {

        // RAII guards for the libhegel handles. Each `*_free` is a no-op on
        // NULL and never throws.
        struct ContextGuard {
            hegel_context_t* ctx = hegel_context_new();
            ContextGuard() = default;
            ~ContextGuard() { hegel_context_free(ctx); }
            ContextGuard(const ContextGuard&) = delete;
            ContextGuard& operator=(const ContextGuard&) = delete;
        };

        struct SettingsGuard {
            hegel_context_t* ctx;
            hegel_settings_t* s = nullptr;
            ~SettingsGuard() { hegel_settings_free(ctx, s); }
        };

        struct RunGuard {
            hegel_context_t* ctx;
            hegel_run_t* run = nullptr;
            ~RunGuard() { hegel_run_free(ctx, run); }
        };

        // Throw with the context diagnostic when a libhegel call fails.
        void check(hegel_context_t* ctx, hegel_result_t rc, const char* what) {
            if (rc != HEGEL_OK) {
                std::string msg = impl::last_error(ctx);
                throw std::runtime_error(std::string(what) + " failed" +
                                         (msg.empty() ? "" : ": " + msg));
            }
        }

        struct BodyOutcome {
            hegel_status_t status;
            std::string origin;
            std::string message;
        };

        // Run the user's test body once and classify the outcome into the
        // libhegel status the caller passes to hegel_mark_complete.
        BodyOutcome run_body(const std::function<void(TestCase&)>& test_fn,
                             TestCase& tc) {
            try {
                test_fn(tc);
                return {HEGEL_STATUS_VALID, "", ""};
            } catch (const internal::HegelStopTest&) {
                return {HEGEL_STATUS_OVERRUN, "", ""};
            } catch (const internal::HegelReject&) {
                return {HEGEL_STATUS_INVALID, "", ""};
            } catch (const std::exception& e) {
                return {HEGEL_STATUS_INTERESTING, typeid(e).name(), e.what()};
            } catch (...) {
                const char* origin = "unknown_exception";
                if (const std::type_info* tinfo =
                        abi::__cxa_current_exception_type()) {
                    origin = tinfo->name();
                }
                return {HEGEL_STATUS_INTERESTING, origin, ""};
            }
        }

        void mark_complete(hegel_context_t* ctx, hegel_test_case_t* tc,
                           const BodyOutcome& outcome) {
            const char* origin =
                outcome.origin.empty() ? nullptr : outcome.origin.c_str();
            check(ctx, hegel_mark_complete(ctx, tc, outcome.status, origin),
                  "hegel_mark_complete");
        }

        // Replay a minimal counterexample blob to reproduce the user's notes
        // and the failing exception's message for display. Returns the
        // message (empty if the blob is stale / produced no exception).
        std::string replay_failure(hegel_context_t* ctx, hegel_settings_t* s,
                                   const char* blob, Verbosity verbosity,
                                   const std::function<void(TestCase&)>& fn) {
            hegel_test_case_t* tc = nullptr;
            hegel_result_t rc = hegel_test_case_from_blob(ctx, s, blob, &tc);
            if (rc != HEGEL_OK || tc == nullptr) {
                return "";
            }
            // Positional init (fields: ctx, tc, is_last_run, verbosity) so this
            // TU stays clean under a C++17 (HEGEL_REFLECTION=OFF) build.
            impl::test_case::TestCaseData data{ctx, tc, /*is_last_run=*/true,
                                               verbosity};
            TestCase tc_obj(&data);
            BodyOutcome outcome = run_body(fn, tc_obj);
            mark_complete(ctx, tc, outcome);
            hegel_test_case_free(ctx, tc);
            return outcome.message;
        }

        // Translate hegel::Settings onto a fresh hegel_settings_t handle.
        void apply_settings(hegel_context_t* ctx, hegel_settings_t* s,
                            const Settings& settings) {
            check(ctx,
                  hegel_settings_set_test_cases(
                      ctx, s, settings.test_cases.value_or(100)),
                  "hegel_settings_set_test_cases");

            hegel_verbosity_t v = HEGEL_VERBOSITY_NORMAL;
            switch (settings.verbosity) {
            case Verbosity::Quiet:
                v = HEGEL_VERBOSITY_QUIET;
                break;
            case Verbosity::Normal:
                v = HEGEL_VERBOSITY_NORMAL;
                break;
            case Verbosity::Verbose:
                v = HEGEL_VERBOSITY_VERBOSE;
                break;
            case Verbosity::Debug:
                v = HEGEL_VERBOSITY_DEBUG;
                break;
            }
            check(ctx, hegel_settings_set_verbosity(ctx, s, v),
                  "hegel_settings_set_verbosity");

            check(ctx,
                  hegel_settings_set_seed(ctx, s, settings.seed.value_or(0),
                                          settings.seed.has_value()),
                  "hegel_settings_set_seed");
            check(ctx,
                  hegel_settings_set_derandomize(ctx, s, settings.derandomize),
                  "hegel_settings_set_derandomize");

            switch (settings.database.kind()) {
            case Database::Kind::Unset:
                break;
            case Database::Kind::Disabled:
                check(ctx, hegel_settings_set_database(ctx, s, ""),
                      "hegel_settings_set_database");
                break;
            case Database::Kind::Path:
                check(ctx,
                      hegel_settings_set_database(
                          ctx, s, settings.database.path().c_str()),
                      "hegel_settings_set_database");
                break;
            }

            uint32_t suppress = 0;
            for (HealthCheck c : settings.suppress_health_check) {
                switch (c) {
                case HealthCheck::FilterTooMuch:
                    suppress |= HEGEL_HC_FILTER_TOO_MUCH;
                    break;
                case HealthCheck::TooSlow:
                    suppress |= HEGEL_HC_TOO_SLOW;
                    break;
                case HealthCheck::TestCasesTooLarge:
                    suppress |= HEGEL_HC_TEST_CASES_TOO_LARGE;
                    break;
                case HealthCheck::LargeInitialTestCase:
                    suppress |= HEGEL_HC_LARGE_INITIAL_TEST_CASE;
                    break;
                }
            }
            if (suppress != 0) {
                check(
                    ctx,
                    hegel_settings_set_suppress_health_check(ctx, s, suppress),
                    "hegel_settings_set_suppress_health_check");
            }
        }

    } // namespace

    void test(const std::function<void(TestCase&)>& test_fn,
              const Settings& settings) {
        impl::protocol::init_protocol_debug(settings.verbosity);

        ContextGuard ctx_guard;
        hegel_context_t* ctx = ctx_guard.ctx;

        SettingsGuard settings_guard{ctx};
        check(ctx, hegel_settings_new(ctx, &settings_guard.s),
              "hegel_settings_new");
        hegel_settings_t* s = settings_guard.s;
        apply_settings(ctx, s, settings);

        RunGuard run_guard{ctx};
        check(ctx, hegel_run_start(ctx, s, &run_guard.run), "hegel_run_start");
        hegel_run_t* run = run_guard.run;

        // Generation loop: pull cases until the engine reports completion
        // (NULL test case), running and marking each.
        while (true) {
            hegel_test_case_t* tc = nullptr;
            check(ctx, hegel_next_test_case(ctx, run, &tc),
                  "hegel_next_test_case");
            if (tc == nullptr) {
                break;
            }
            impl::test_case::TestCaseData data{ctx, tc, /*is_last_run=*/false,
                                               settings.verbosity};
            TestCase tc_obj(&data);
            BodyOutcome outcome = run_body(test_fn, tc_obj);
            mark_complete(ctx, tc, outcome);
        }

        const hegel_run_result_t* result = nullptr;
        check(ctx, hegel_run_result(ctx, run, &result), "hegel_run_result");

        hegel_run_status_t run_status = HEGEL_RUN_STATUS_PASSED;
        check(ctx, hegel_run_result_status(ctx, result, &run_status),
              "hegel_run_result_status");

        if (run_status == HEGEL_RUN_STATUS_PASSED) {
            return;
        }

        if (run_status == HEGEL_RUN_STATUS_ERROR) {
            // The run itself failed (health check, nondeterminism, engine
            // panic) and produced no verdict on the property.
            const char* run_err = nullptr;
            check(ctx, hegel_run_result_error(ctx, result, &run_err),
                  "hegel_run_result_error");
            throw std::runtime_error(std::string("Hegel run error: ") +
                                     (run_err ? run_err : "unknown error"));
        }

        // Failed: replay each distinct counterexample to surface its notes and
        // exception message, then raise.
        size_t failure_count = 0;
        check(ctx, hegel_run_result_failure_count(ctx, result, &failure_count),
              "hegel_run_result_failure_count");

        std::string message;
        for (size_t i = 0; i < failure_count; i++) {
            const hegel_failure_t* failure = nullptr;
            check(ctx, hegel_run_result_failure(ctx, result, i, &failure),
                  "hegel_run_result_failure");
            const char* blob = nullptr;
            check(ctx, hegel_failure_reproduction_blob(ctx, failure, &blob),
                  "hegel_failure_reproduction_blob");
            if (blob == nullptr) {
                continue;
            }
            std::string replayed =
                replay_failure(ctx, s, blob, settings.verbosity, test_fn);
            if (message.empty() && !replayed.empty()) {
                message = replayed;
            }
        }

        throw std::runtime_error("\nHegel test failed" +
                                 (message.empty() ? "" : ": " + message));
    }

} // namespace hegel
