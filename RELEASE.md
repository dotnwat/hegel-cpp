RELEASE_TYPE: minor

Bumps our pinned `hegel-core` to
[0.5.0](https://github.com/hegeldev/hegel-core/releases/tag/v0.5.0) and
adopts its new `one_of` wire format (the server now returns `[index, value]`
for `one_of` schemas, removing the need for per-branch tagged-tuple
wrapping). This is a transparent change to API users, but requires
`hegel-core >= 0.5.0` at runtime.
