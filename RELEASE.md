RELEASE_TYPE: patch

This patch adds three `TestCase` methods and three generators.

New `TestCase` methods:

- `tc.reject()` rejects the current test case unconditionally. Unlike `tc.assume(false)`, it is marked `[[noreturn]]` so it can stand in for a value in a branch that cannot continue.

- `tc.target(score, label = "")` records a numeric observation for the engine's targeted-search phase to maximize. Higher scores are treated as more interesting, so the engine biases later test cases toward inputs that produced higher scores under the same label.

- `tc.repeat(body)` runs `body` in an engine-managed loop whose iteration count the engine chooses and shrinks, like any other drawn value.

```cpp
int total = 0;
tc.repeat([&] {
    total += tc.draw(gs::integers<int>({.min_value = 0, .max_value = 10}));
    if (total >= 50) throw std::runtime_error("too much");
});
```

New generators:

- `uuids()` produces canonical hyphenated UUID strings (e.g. `f47ac10b-58cc-4372-a567-0e02b2c3d479`). By default any version is generated. Pass `{.version = N}` to force an RFC 4122 version (1-5).

- `arrays<T, N>(element)` produces a fixed-size `std::array<T, N>`, drawing exactly `N` elements from the generator 
`element`. Unlike `vectors()`, the length is fixed at compile time, so `N` is given explicitly: `arrays<int, 3>(integers<int>())`.

- `deferred<T>()` creates a forward reference for recursive or mutually recursive generators. Call `.generator()` to obtain handles before the implementation is known, embed them in other generators, then call `.set(...)` once to install the implementation:

```cpp
struct Tree { int leaf; std::vector<Tree> children; };

auto tree = gs::deferred<Tree>();
auto leaf = gs::integers<int>().map([](int v) { return Tree{v, {}}; });
auto branch = gs::compose([tree](const hegel::TestCase& tc) {
    return Tree{0, {tc.draw(tree.generator()), tc.draw(tree.generator())}};
});
tree.set(gs::one_of<Tree>({leaf, branch}));
```
