RELEASE_TYPE: patch

Update the wire schemas emitted by `optional` and `ip_addresses` to match
the latest `hegel-core` protocol: `optional` now emits
`{"type": "constant", "value": null}` for the null branch (replacing
`{"type": "null"}`), and `ip_addresses` now emits
`{"type": "ip_address", "version": N}` (replacing `{"type": "ipv4"}` /
`{"type": "ipv6"}`). No public C++ API change.

Bump our pinned hegel-core to [0.6.0](https://github.com/hegeldev/hegel-core/releases/tag/v0.6.0), incorporating the following change:

> This release makes the following breaking protocol changes:
> - Removed `{"type": "sampled_from"}`. Instead of serializing the values to sample from, ask for an integer index and index into the collection of values on the client side.
> - Removed `{"type": "null"}`. Use `{"type": "constant", "value": null}` instead.
> - Replaced `{"type": "ipv4"}` and `{"type": "ipv6"}` with a single `{"type": "ip_address", "version": <4|6>}` schema.
>
> The protocol version is now 0.12.
>
> — [v0.6.0](https://github.com/hegeldev/hegel-core/releases/tag/v0.6.0)
