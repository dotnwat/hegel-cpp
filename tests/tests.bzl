"""Test helper that mirrors tests/CMakeLists.txt's hegel_add_test() function."""

load("@rules_cc//cc:defs.bzl", "cc_test")

def hegel_cc_test(
        name,
        src,
        use_utils = False,
        internal = False,
        approvals = False,
        **kwargs):
    """Registers one GTest-based test executable.

    Args:
      name: target name.
      src: the test .cpp file.
      use_utils: link the shared test utilities and add tests/ to the
        include path (for "common/utils.h").
      internal: add src/ to the include path, for unit tests against
        private headers.
      approvals: the test contains approval (snapshot) tests: the executable
        uses common/approval_main.cpp as its main, links ApprovalTests
        instead of gtest_main, and gets the approved snapshots as runfiles.
      **kwargs: forwarded to cc_test.
    """
    srcs = [src]
    deps = ["//:hegel", "@googletest//:gtest"]
    copts = kwargs.pop("copts", [])
    data = kwargs.pop("data", [])
    if use_utils:
        deps.append(":common")
    if approvals:
        srcs.append("common/approval_main.cpp")
        deps.append("@approvaltests")
        # ApprovalTests locates the approved snapshots relative to the test
        # source file, so the source must be in the runfiles too.
        data = data + [src, ":approval_files"]
    else:
        deps.append("@googletest//:gtest_main")
    if internal:
        copts = copts + ["-Isrc"]
    kwargs.setdefault("size", "small")
    cc_test(
        name = name,
        srcs = srcs,
        copts = copts,
        data = data,
        deps = deps,
        **kwargs
    )
