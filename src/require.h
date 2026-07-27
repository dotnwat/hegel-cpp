#pragma once

/**
 * @cond INTERNAL
 */

// Lines up the parts of two composite values, so the difference of two values
// can point at the parts that changed. Used by
// hegel::internal::render_parts_diff.

#include <cstddef>
#include <string>
#include <vector>

namespace hegel::impl::require {

    // Indexes into `left` and `right` that a longest common subsequence
    // matches to each other, in increasing order.
    struct Match {
        size_t left;
        size_t right;
    };

    std::vector<Match>
    common_subsequence(const std::vector<std::string>& left,
                       const std::vector<std::string>& right);

} // namespace hegel::impl::require

/// @endcond
