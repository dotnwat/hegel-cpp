RELEASE_TYPE: patch

This release adds `HEGEL_TEST`, the new recommended way to define a property test. The macro defines the test as a plain function you can invoke from `main()` or any test framework, and derives a database key from the defining file and test name, so failing examples are persisted to the example database and replayed first on later runs. Settings can be written inline after the test name and become the function's default argument. Settings passed when invoking the test replaces them for that run:

```cpp
HEGEL_TEST(addition_commutes, {.test_cases = 500})(hegel::TestCase& tc) {
    int x = tc.draw(gs::integers<int>());
    int y = tc.draw(gs::integers<int>());
    if (x + y != y + x) {
        throw std::runtime_error("addition is not commutative");
    }
}
```

Each `HEGEL_TEST` is also registered with the new `hegel::run_all_tests()`, which runs every test defined with the macro in a translation unit. It reports failures to stderr and returns 0 if everything passed (1 otherwise).

You can run the test above in two ways:

```cpp
int main() {
    return hegel::run_all_tests();
}
```
```cpp
int main() {
    addition_commutes();
    return 0;
}
```

There are four new settings:

- `database_key`: the key scoping which examples are stored in and replayed from the database. `HEGEL_TEST` fills it in automatically. Set it yourself when calling `hegel::test()` directly.
- `phases`: which phases of the run to enable (`Phase::Explicit`, `Reuse`, `Generate`, `Target`, `Shrink`)
- `mode`: `Mode::TestRun` (the default) or `Mode::SingleTestCase`, which produces one test case and stops with no shrinking intended for long running tests
- `backend`: the engine's randomness source. `Backend::Auto` (the default), `Backend::Default` (seeded PRNG), or `Backend::Urandom` (fresh entropy per draw, intended for Antithesis)
