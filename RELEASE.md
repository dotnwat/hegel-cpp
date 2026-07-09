RELEASE_TYPE: major

The default match mode of `from_regex` is now fullmatch: `from_regex(pattern)` generates strings where the entire string matches the pattern, as if anchored with `^...$`. Previously the generated string only needed to *contain* a match, so arbitrary characters could surround it.

```cpp
auto g = from_regex("[A-Z]{2}-[0-9]{4}");
// before: "xx QX-8271 yy"  (string need only contain a match)
// after:  "QX-8271"        (entire string matches, as if anchored with ^...$)

from_regex("[A-Z]{2}-[0-9]{4}", false);  // restores the old behavior
```
