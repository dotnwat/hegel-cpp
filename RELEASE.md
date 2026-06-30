RELEASE_TYPE: minor

This release replaces the Python hegel-core engine with the `libhegel` Rust engine,
called in-process through its C ABI. There is no longer a subprocess, socket,
or wire protocol, and `uv` is no longer required.

The public generator and `hegel::test` API is unchanged.

This release also adds a C++17 build path. Configure with `-DHEGEL_REFLECTION=OFF`
to drop the reflect-cpp dependency and build at C++17. `default_generator` and
automatic struct derivation become unavailable, but every other generator and
combinator still works.
