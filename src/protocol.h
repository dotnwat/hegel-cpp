#pragma once

#include <hegel/settings.h>

namespace hegel::impl::protocol {

    // Per-draw REQUEST/RESPONSE tracing, enabled at Debug verbosity or via
    // the HEGEL_PROTOCOL_DEBUG environment variable.

    void set_protocol_debug(bool enabled);
    bool protocol_debug_enabled();

    /// Initialize protocol debug flag from verbosity + env var
    void init_protocol_debug(Verbosity verbosity);

} // namespace hegel::impl::protocol
