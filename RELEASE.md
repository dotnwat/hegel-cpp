RELEASE_TYPE: minor

This release replaces the Hegel test framework with an opt-in GTest integration.
`gtest_throw_on_failure` is no longer needed for a property to see a failed 
assertion. Users can still call existing Hegel tests as regular functions from 
`main()`.

 `HEGEL_DRAW`, `HEGEL_REQUIRE_EQUAL`, `HEGEL_REQUIRE`, `HEGEL_FAIL`, 
 `hegel::run_all_tests()`, and the test registry have been removed. 

Hegel can now distinguish between multiple exceptions of the same type. Their
locations in the test function is the differentiator by default. 
Derive exceptions from `hegel::FailureOrigin` to differentiate them some other
way.
