RELEASE_TYPE: minor

This release adds the `report_multiple_failures` setting and reworks how
`hegel::test` reports failures.

With `report_multiple_failures` set to `true`, the engine keeps generating
after the first failure to surface additional distinct bugs. The setting 
defaults to `false`.

A test with a single failing example now re-raises the test's own exception
instead of `std::runtime_error`.

Foreign (non-C++) exceptions escaping a test body are now reported as a
`std::runtime_error` instead of undefined behavior.
