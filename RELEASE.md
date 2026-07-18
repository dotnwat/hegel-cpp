RELEASE_TYPE: patch

This patch adds stateful testing under the `hegel::stateful` namespace. A stateful test drives the system under test through a sequence of randomly chosen actions applied to a state. When a sequence falsifies an invariant or throws, the engine shrinks it to a minimal failing sequence and replays it.

Define actions with `hegel::stateful::Rule<T>` and optionally, invariants with `hegel::stateful::Invariant<T>`, then run the machine with `hegel::stateful::run`. A rule's step mutates the state in place. Invariants are evaluated before the first step and after every valid step.

```cpp
namespace gs = hegel::generators;

hegel::test([](hegel::TestCase& tc) {
    auto push = hegel::stateful::Rule<std::vector<int>>(
        "push", [](hegel::TestCase& tc, std::vector<int>& stack) {
            stack.push_back(
                tc.draw(gs::integers<int>({.min_value = 0, .max_value = 100})));
        });
    auto pop = hegel::stateful::Rule<std::vector<int>>(
        "pop", [](hegel::TestCase& tc, std::vector<int>& stack) {
            tc.assume(!stack.empty());
            stack.pop_back();
        });

    std::vector<int> stack;
    hegel::stateful::run(tc, stack, {push, pop}, {});
});
```
