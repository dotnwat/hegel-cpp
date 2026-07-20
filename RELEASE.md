RELEASE_TYPE: minor

This release changes how drawn values are printed in failure replays and
verbose output. Hegel previously printed one `Generated: <value>` line per
primitive draw inside the engine. It now prints one line per user-level `tc.draw(...)`, 
with the final composed value rendered as a C++ declaration:

```
auto draw_1 = std::vector<int>{0, 0};
```

Types Hegel cannot render as an expression fall back to
their `operator<<` output, then to an `<unprintable Type>` placeholder.

This release also adds named draws. `tc.draw("x", gen)` prints the draw
under the given name, and the new `HEGEL_DRAW` macro binds a variable and
captures its name in one step:

```cpp
HEGEL_TEST(addition_commutes)(hegel::TestCase& tc) {
    HEGEL_DRAW(tc, x, gs::integers<int>());
    HEGEL_DRAW(tc, y, gs::integers<int>());
    if (x + y != y + x) throw std::runtime_error("not commutative");
}
// replay output: auto x = 10;
//                auto y = 3;
```

A name prints bare on every use. Pass `repeatable = true`
(`tc.draw("x", gen, true)`) to number repeated draws `x_1`, `x_2`, ...
for draws in loops. Unnamed draws print as `draw_1`, `draw_2`, ...
per test case. In stateful tests, each rule's draws print indented under
their `Step N:` header.
