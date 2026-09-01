#!/usr/bin/env bash
# Runs a test binary that fails by design and passes when its output shows
# the counterexample. Equivalent to the CTest PASS_REGULAR_EXPRESSION
# "actual:" property on the test_gtest CMake target.
set -u

out="$("$1" 2>&1)" || true
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -q "actual:"
