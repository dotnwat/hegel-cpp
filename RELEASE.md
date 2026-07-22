RELEASE_TYPE: patch

This patch adds test-case cloning. `TestCase::clone()` forks an independent draw stream of the current test case. The clone draws from its own choice sequence but shares the case's outcome and budget. A single test case must not be drawn from concurrently.

`TestCase::spawn()` runs a callable on a clone in a new thread and returns a `hegel::Worker`. `Worker::join()` awaits it, returning the callable's result and re-raising any exception it threw. Join every worker before the test body returns.

```cpp
auto worker = tc.spawn([](hegel::TestCase& c) {
    return c.draw(gs::integers<int>());
});
auto mine = tc.draw(gs::integers<int>());
auto theirs = worker.join();
```
