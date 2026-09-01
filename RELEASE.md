RELEASE_TYPE: patch

This patch adds the `HEGEL_THROW_SITE` CMake option (default `ON`). Hegel
names the source position of a failing throw by wrapping `__cxa_throw` and
forwarding to the real one through `dlsym(RTLD_NEXT, ...)`, which requires
the C++ runtime to be a shared library. Toolchains that link the C++ runtime
statically (for example `-static-libstdc++`, or a hermetic toolchain with
`libc++abi.a`) previously failed to link with a duplicate `__cxa_throw`
definition. Configure with `-DHEGEL_THROW_SITE=OFF` (or, outside CMake,
compile the library with `HEGEL_HAS_THROW_SITE=0`) to compile the wrapper
out; failure reports then name the exception type without the throw site.
