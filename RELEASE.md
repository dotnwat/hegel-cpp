RELEASE_TYPE: minor

This release overhauls what Hegel prints when a property fails, adds three
macros for stating what a test requires, and changes how a state machine
declares its state to accommodate state printing.

A failing run now produces a framed report like the one below:

```
--- Failure: sort_agrees_with_std_sort (sort_test.cpp:9) ----------------
Falsified after 8 test cases (0 discarded):

  auto vec1 = std::vector<int>{0, 0};

Exception: std::runtime_error: sort mismatch
rerun with: HEGEL_REPRODUCE_FAILURE(sort_agrees_with_std_sort, "AXicY2VgYGBkZOBiZEBhMAAAAd8AIQ==")
```

`Settings::print_blob` now defaults to `true`, so the `rerun with:` line
prints by default. A run that finds several distinct failures reports each in
its own numbered section.

`HEGEL_TEST` supplies the test's name and source line, so its failures name
themselves in the header. `hegel::test()` now optionally takes a `TestLocation`.

```cpp
hegel::test(my_body, {"my_property", __FILE__, __LINE__}, {.test_cases = 500});
```

This release also adds `HEGEL_REQUIRE`, `HEGEL_REQUIRE_EQUAL`, and
`HEGEL_FAIL` for stating properties and failures.

```cpp
HEGEL_TEST(addition_commutes)(hegel::TestCase& tc) {
    HEGEL_DRAW(tc, x, gs::integers<int>());
    HEGEL_DRAW(tc, y, gs::integers<int>());
    HEGEL_REQUIRE_EQUAL(tc, x + y, y + x);
}
```

For an equality property prefer `HEGEL_REQUIRE_EQUAL`. Its report shows a
difference of the two values:

```
HEGEL_REQUIRE_EQUAL: values differ (- lhs / + rhs):
  Team{
    .name = std::string("a"),
    .scores = std::vector<int>{
      1,
-     2,
+     9,
      3,
    },
  }
```

Prefer all three over a raw `throw`. Hegel tells one bug from another by
where the failure came from, and a raw `throw` carries no source position, so 
every throw of one type in a run counts as a single bug under 
`Settings::report_multiple_failures`. Each macro invocation carries the position
of the line you wrote it on, so failures on different lines are distinct.

`StateMachine` now takes the state's type as a second template argument and 
holds the state itself, and its constructor takes the initial state. A failing 
stateful run prints that state before the first step and after every step that
runs to completion.

```cpp
struct Stack : hegel::stateful::StateMachine<Stack, std::vector<int>> {
    Stack() : StateMachine({.initial_state = {}}) {}

    std::vector<hegel::stateful::Rule<Stack>> rules() {
        return {hegel::stateful::Rule<Stack>(
            "push", [](hegel::TestCase& tc, Stack& m) {
                m.state.push_back(tc.draw(gs::integers<int>()));
            })};
    }
};
```

State printing can be disabled with `.print_state = false`.

```cpp
hegel::stateful::run(machine, tc, {.print_state = false});
```
