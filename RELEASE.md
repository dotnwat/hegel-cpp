RELEASE_TYPE: patch

This patch adds stateful testing under the `hegel::stateful` namespace. A stateful test drives the system under test through a sequence of randomly chosen actions applied to a state machine. When a sequence falsifies an invariant or throws, the engine shrinks it to a minimal failing sequence and replays it.

Derive a machine from `hegel::stateful::StateMachine<T>` and define its `rules()`, returning `hegel::stateful::Rule<T>` actions. Optionally override `invariants()` to add `hegel::stateful::Invariant<T>` predicates. Pass an instance to `hegel::stateful::run`. A rule's step mutates the machine in place. Invariants are evaluated before the first step and after every valid step.

```cpp
namespace gs = hegel::generators;

struct IntegerStack : hegel::stateful::StateMachine<IntegerStack> {
    std::vector<int> stack;

    std::vector<hegel::stateful::Rule<IntegerStack>> rules() {
        return {
            hegel::stateful::Rule<IntegerStack>(
                "push", [](hegel::TestCase& tc, IntegerStack& m) {
                    m.stack.push_back(tc.draw(
                        gs::integers<int>({.min_value = 0, .max_value = 100})));
                }),
            hegel::stateful::Rule<IntegerStack>(
                "pop", [](hegel::TestCase& tc, IntegerStack& m) {
                    tc.assume(!m.stack.empty());
                    m.stack.pop_back();
                }),
        };
    }
};

hegel::test([](hegel::TestCase& tc) {
    IntegerStack machine;
    hegel::stateful::run(machine, tc);
});
```
