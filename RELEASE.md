RELEASE_TYPE: patch

Bump our pinned [`hegel-core`](https://github.com/hegeldev/hegel-core) version from `0.4.0` to [`0.4.14`](https://github.com/hegeldev/hegel-core/releases/tag/v0.4.14), incorporating the following changes:

- **0.4.2** — adds `crash_after_handshake` and `crash_after_handshake_with_stderr` test modes for exercising client crash detection without reimplementing the binary protocol.
- **0.4.6** — fixes several concurrency bugs and improves error handling in the protocol layer. The server's reader loop no longer crashes on packets for unknown/closed streams or malformed close-stream packets, and instead replies with a `ProtocolError`. Several races around `Connection.close()`, `Stream.close()`, `Stream.write_request`, `Connection.new_stream`, and `receive_handshake` are fixed. Bare `assert` statements are replaced with explicit error raises with descriptive messages. `StdioTransport.sendall` now converts `ValueError` from a closed fd to `OSError` so existing handling catches it.
- **0.4.7** — adds a `single_test_case` top-level protocol command that hands the client a single final-mode test case with no shrinking or replay.
- **0.4.8** — removes the unused Unix socket transport from the server. The server now always communicates over stdin/stdout, matching how all current libraries (including this one) spawn it. We drop the `--stdio` flag we used to pass on the server command line, since the server no longer accepts it.
- **0.4.10** — adds fraction and complex number schema types. (No hegel-cpp generator emits these yet, so this is a no-op for current users; we'll start exposing them in a follow-up.)
- **0.4.12** — removes CBOR tagging from fraction and complex numbers.
- **0.4.14** — pins core dependencies below their next major version.

Other releases in this range (`0.4.1`, `0.4.3`, `0.4.4`, `0.4.5`, `0.4.9`, `0.4.11`, `0.4.13`) only touched the conformance-testing harness in hegel-core and have no user-visible effect on hegel-cpp.

This release also adds a `hegel-core-release` `repository_dispatch` receiver workflow (`.github/workflows/bump-hegel-core.yml` + `.github/scripts/bump_hegel_core.py`), so that future hegel-core releases will automatically open a bump PR against this repo.
