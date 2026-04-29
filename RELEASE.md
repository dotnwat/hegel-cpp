RELEASE_TYPE: minor

`one_of` no longer wraps children in tagged tuples; it relies on the new protocol contract where the server emits `[index, value]` for `one_of` schemas. Requires `hegel >= <next-version>`.
