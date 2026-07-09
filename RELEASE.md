RELEASE_TYPE: patch

This patch adds the ability to reproduce a specific failing example from its
reproduction blob and a `print_blob` setting to toggle printing those blobs.

To replay a failure, annotate a `HEGEL_TEST` with `HEGEL_REPRODUCE_FAILURE`. 
The test replays that blob instead of generating new cases:

```cpp
HEGEL_REPRODUCE_FAILURE(my_property, "AAEAAAAACgEAAAAA")
HEGEL_TEST(my_property)(hegel::TestCase& tc) {
    int n = tc.draw(gs::integers<int>());
    if (n < 50) {
        throw std::runtime_error("fail");
    }
}
```

At least one blob is required. More than one blob can be added for bookkeeping but 
only the first is replayed. Delete the annotation to return to a normal run.

When calling `hegel::test` directly, pass the blobs as the third argument:

```cpp
hegel::test(my_property, {}, {"AAEAAAAACgEAAAAA"});
```
