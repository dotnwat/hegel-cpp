RELEASE_TYPE: patch

This patch adds concurrent stateful testing: stateful tests where rules run concurrently from a number of worker threads. See the documentation for
`run_concurrent` and `ConcurrentStateMachine` for details.

It also adds `ConcurrentPool<T>`, which allows concurrent rules to safely add,
reuse, and consume shared generated values. Stateful invariants now run in full on the initial and final states and are sampled between rules in sequential stateful tests and between rounds (a set of concurrent rule executions) in concurrent stateful tests.
