# hegel-rust parity TODO

Gaps in hegel-cpp relative to hegel-rust (the reference implementation),
found by auditing both API surfaces and diffing the C ABI (`libhegel/hegel.h`)
against what `src/` actually calls. Ordered roughly by suggested priority:
mechanical plumbing first, then high-value features, then the big subsystems.

Last updated: 2026-07-07.

ENFORCE 100% COVERAGE WITH `just check-coverage`

## Done

- [x] Multiple-failure reporting: `report_multiple_failures` setting, one
      `Failure N:` block per distinct bug, count-carrying final exception,
      single failure re-raises the test's own exception (mirrors
      `run_lifecycle.rs::drive`).
      Deliberate divergence: defaults to `false` here; hegel-rust and the
      engine default to `true`.
- [x] `hegel_failure_origin` surfaced in reporting: the `Exception <type>:`
      line reads the engine's grouped origin (demangled at capture in
      `run_body`, so origins are readable engine-side too).
- [x] Structural spans — every combinator now emits its shrink label
      (LIST/SET/MAP via collections, TUPLE, ONE_OF, OPTIONAL, FLAT_MAP,
      FILTER with discard-on-reject, MAPPED), mirroring the Rust
      generators. Landed with the libhegel 0.27.0 typed-draw migration.
- [x] Engine collection protocol — `vectors`/`sets`/`maps` drive
      `hegel_new_collection` / `hegel_collection_more` /
      `hegel_collection_reject` (duplicates rejected back to the engine);
      adaptive engine-managed sizing replaced the hardcoded fallback caps.

## Settings passthroughs (mechanical ABI plumbing)

- [x] `phases` — `hegel_settings_set_phases`; no way to skip shrinking etc.
      (Rust: `Explicit | Reuse | Generate | Target | Shrink`).
- [x] `mode` — `hegel_settings_set_mode`; `SingleTestCase` vs full run.
- [x] `backend` — `hegel_settings_set_backend`; `Default` vs `Urandom`.
- [x] `database_key` — `hegel_settings_set_database_key`; scopes Reuse-phase
      replay. `tests/test_settings.cpp` `DatabaseReplaysFailure` unwrapped,
      Rust isolation check ported (different key → no replay). `HEGEL_TEST`
      macro (issue #31) derives the key from `__FILE__` + test name.

## Targeted PBT + Antithesis (small surface, high value)

- [x] `TestCase::target(score)` (+ labelled variant) — `hegel_target`;
      guided by the Target phase. Single `target(score, label = "")` method;
      routes rc through the shared `DrawScope::raise_for_rc`.
- [ ] Antithesis integration: urandom backend auto-selection under
      `ANTITHESIS_OUTPUT_DIR`, `sdk.jsonl` always-assertion emission,
      SingleTestCase mode for workloads (Rust: `src/antithesis.rs`).

## Failure workflow

- [X] Reproduce-failure blobs: user-facing API to print (`print_blob`) and
      supply a reproduction blob (Rust: `#[hegel::reproduce_failure]`).
      Blobs already flow internally through `replay_failure`.

## Stateful testing (largest new surface; full engine support unused)

- [X] Value pools — `hegel_new_pool`, `hegel_pool_add`,
      `hegel_pool_generate` (Rust: `Pool<T>` with reusable/consumed draws).
- [ ] State machines — `hegel_new_state_machine`,
      `hegel_state_machine_next_rule`, rules + invariants runner (Rust:
      `stateful.rs`, `#[hegel::state_machine]`). Includes swarm testing via
      `HEGEL_LABEL_FEATURE_FLAG`.

## Generators

- [x] `uuids()` (versions 1–5). `UuidsParams{version}`; version validated
      1–5 at construction (matches `ip_addresses` idiom).
- [x] `deferred()` — recursive / forward-reference generators.
      `deferred<T>()` → definition with `generator()` / `set()`; `set()` is
      once-only.
- [x] Fixed-size `std::array` generator (Rust: `arrays::<G, T, N>()`).
      `arrays<T, N>(element)` (N explicit); N draws in one tuple span.

## TestCase API

- [x] `reject()` sugar for `assume(false)`. `[[noreturn]]`.
- [x] `repeat()` engine-managed loop.
- [ ] Named-draw capture for failure output (Rust's `#[hegel::test]`
      rewrites `draw` to record the variable name).
- [ ] Draw-output granularity: `Generated:` lines are emitted per
      *primitive* draw inside the engine wrappers (one line per element of
      a `vectors()` draw, raw structs for dates/times), where Rust's
      `on_draw` sink prints one `let x_0 = <value>;` line per user-level
      `tc.draw(...)` with the final composed value. Moving the logging up
      to `TestCase::draw` gets the composed-value granularity and is the
      natural home for named-draw capture above (the source of the `x_0`
      names) — but unlike Rust's `{:?}`, C++ can't render arbitrary `T`,
      so it needs a printer story (operator<< detection with a fallback,
      or reflect-cpp when available). `tests/test_output.cpp` pins the
      current per-primitive lines and would move with it.
