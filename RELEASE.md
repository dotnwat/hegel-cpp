RELEASE_TYPE: minor

This release bumps our pinned `libhegel` ([hegel-rust](hegeldev/hegel-rust)) from [0.23.2](https://github.com/hegeldev/hegel-rust/releases/tag/v0.23.2) to [0.27.0](https://github.com/hegeldev/hegel-rust/releases/tag/v0.27.0) and migrates the library to its reworked C ABI.

The CBOR request/response protocol has been removed, so the `HEGEL_PROTOCOL_DEBUG` environment variable and the Debug-verbosity `REQUEST:`/`RESPONSE:` dump have been removed. `Verbosity::Debug` now enables the engine's own shrinker tracing.
