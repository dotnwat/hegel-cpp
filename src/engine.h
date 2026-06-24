#pragma once

/**
 * @cond INTERNAL
 *
 * Thin helpers over the libhegel C ABI shared by the run loop (hegel.cpp)
 * and the per-draw path (engine.cpp).
 */

#include <hegel.h>
#include <string>

namespace hegel::impl {

    /// Most recent error message recorded on `ctx` (empty string if none).
    std::string last_error(hegel_context_t* ctx);

} // namespace hegel::impl

/// @endcond
