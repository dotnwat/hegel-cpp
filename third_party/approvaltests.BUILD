load("@rules_cc//cc:defs.bzl", "cc_library")

# ApprovalTests.cpp as the CMake build consumes it. Both the repository root
# and the library directory go on the include path, so <ApprovalTests.hpp>
# and "ApprovalTests/..." includes both resolve.
cc_library(
    name = "approvaltests",
    srcs = glob(["ApprovalTests/**/*.cpp"]),
    hdrs = glob([
        "ApprovalTests/**/*.h",
        "ApprovalTests/**/*.hpp",
    ]),
    includes = [
        ".",
        "ApprovalTests",
    ],
    visibility = ["//visibility:public"],
)
