# Downloads the prebuilt libhegel shared library for each released platform.
#
# Keep the version and the SHA-256 hashes in sync with cmake/libhegel.cmake,
# libhegel/hegel.h, and nix/flake.nix. darwin/amd64 is intentionally absent:
# no prebuilt libhegel is published for it.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

LIBHEGEL_VERSION = "0.34.0"

# Platform -> (file extension, SHA-256 of the released asset).
_ASSETS = {
    "darwin-arm64": ("dylib", "c8219553a3a363a90a185f517f25fbf726d81a3bd0c4f1cf2154ad9edf912708"),
    "linux-amd64": ("so", "bd7e15e54d089aa3554cb3e5744ae7c38b1df58fd0679a723d47ab3958273593"),
    "linux-arm64": ("so", "c0627d86fafff22a250ad084f1fd853ed02bbd3a1dd0a847f2584eb15e2401eb"),
}

def _libhegel_impl(_module_ctx):
    for platform, info in _ASSETS.items():
        ext = info[0]
        sha256 = info[1]
        http_file(
            name = "libhegel-" + platform,
            # The on-disk name matches the Rust output stem (libhegel_c.<ext>)
            # so the Linux SONAME and the macOS @rpath name both resolve to
            # this file.
            downloaded_file_path = "libhegel_c." + ext,
            sha256 = sha256,
            url = "https://github.com/hegeldev/hegel-rust/releases/download/v{version}/libhegel-{platform}.{ext}".format(
                version = LIBHEGEL_VERSION,
                platform = platform,
                ext = ext,
            ),
        )

libhegel = module_extension(implementation = _libhegel_impl)
