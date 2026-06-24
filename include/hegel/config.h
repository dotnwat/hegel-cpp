#pragma once

/**
 * @file config.h
 * @brief Compile-time feature configuration.
 */

/**
 * @def HEGEL_HAS_REFLECTION
 * @brief Whether the reflect-cpp powered features are available.
 *
 * Gates @ref hegel::generators::default_generator and the automatic parsing of
 * reflectable structs. The CMake build defines this: `1` when built with
 * `HEGEL_REFLECTION=ON` (the default, which also requires C++20), `0`
 * otherwise. When the macro is not provided — e.g. the headers are used
 * outside the CMake target — it falls back to whether the compiler advertises
 * C++20 concepts, which reflect-cpp requires.
 *
 * Building with `-DHEGEL_REFLECTION=OFF` drops reflect-cpp and lets the library
 * be consumed from C++17; everything except `default_generator` (and the
 * automatic struct parser it relies on) still works.
 */
#ifndef HEGEL_HAS_REFLECTION
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
#define HEGEL_HAS_REFLECTION 1
#else
#define HEGEL_HAS_REFLECTION 0
#endif
#endif

/**
 * @def HEGEL_REQUIRES
 * @brief A requires-clause under C++20, and nothing under C++17.
 *
 * Constrained templates pair this with a `static_assert` in the body so misuse
 * is still rejected with a clear message when concepts are unavailable.
 */
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
#define HEGEL_REQUIRES(...) requires(__VA_ARGS__)
#else
#define HEGEL_REQUIRES(...)
#endif

namespace hegel::internal {
    /// @cond INTERNAL
    /// Always-false dependent value, for `static_assert` in discarded
    /// `if constexpr` / `#if` branches.
    template <typename...> inline constexpr bool always_false_v = false;
    /// @endcond
} // namespace hegel::internal
