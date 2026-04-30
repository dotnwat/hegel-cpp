RELEASE_TYPE: patch

Update the wire schemas emitted by `optional` and `ip_addresses` to match
the latest `hegel-core` protocol: `optional` now emits
`{"type": "constant", "value": null}` for the null branch (replacing
`{"type": "null"}`), and `ip_addresses` now emits
`{"type": "ip_address", "version": N}` (replacing `{"type": "ipv4"}` /
`{"type": "ipv6"}`). No public C++ API change.
