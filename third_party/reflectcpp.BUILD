load("@rules_cc//cc:defs.bzl", "cc_library")

# reflect-cpp configured the same way as the CMake build: the JSON-only core
# with the bundled yyjson and ctre, no optional formats, no extra defines.
# The upstream src/reflectcpp*.cpp files are unity wrappers around these
# sources; compiling the sources directly keeps the target self-contained.
cc_library(
    name = "reflectcpp",
    srcs = [
        "src/rfl/Generic.cpp",
        "src/rfl/generic/Writer.cpp",
        "src/rfl/internal/strings/strings.cpp",
        "src/rfl/json/Writer.cpp",
        "src/rfl/json/to_schema.cpp",
        "src/rfl/parsing/schema/Type.cpp",
        "src/rfl/parsing/schemaful/tuple_to_object.cpp",
        "src/yyjson.c",
    ],
    hdrs = glob([
        "include/**/*.h",
        "include/**/*.hpp",
    ]),
    includes = [
        "include",
        "include/rfl/thirdparty",
    ],
    visibility = ["//visibility:public"],
)
