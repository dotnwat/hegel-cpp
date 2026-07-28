// Implementations behind HEGEL_FAIL, HEGEL_REQUIRE, and HEGEL_REQUIRE_EQUAL:
// the failure itself, the note channel the difference prints through, and the
// difference of two values that the failure report shows.

#include <hegel/internal.h>
#include <hegel/test_case.h>

#include <hegel/repr.h>

#include <require.h>
#include <test_case.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace hegel::impl::require {

    std::vector<Match>
    common_subsequence(const std::vector<std::string>& left,
                       const std::vector<std::string>& right) {
        size_t rows = left.size();
        size_t cols = right.size();
        // lengths[i][j] is the length of the longest common subsequence of
        // the suffixes left[i..] and right[j..].
        std::vector<std::vector<size_t>> lengths(
            rows + 1, std::vector<size_t>(cols + 1, 0));
        for (size_t i = rows; i-- > 0;) {
            for (size_t j = cols; j-- > 0;) {
                lengths[i][j] =
                    left[i] == right[j]
                        ? lengths[i + 1][j + 1] + 1
                        : std::max(lengths[i + 1][j], lengths[i][j + 1]);
            }
        }

        std::vector<Match> matches;
        size_t i = 0;
        size_t j = 0;
        while (i < rows && j < cols) {
            if (left[i] == right[j]) {
                matches.push_back({i, j});
                i++;
                j++;
            } else if (lengths[i + 1][j] >= lengths[i][j + 1]) {
                i++;
            } else {
                j++;
            }
        }
        return matches;
    }

} // namespace hegel::impl::require

namespace hegel::internal {

    namespace {
        // One line of a difference: a marker column, then the content at
        // `depth` levels of two-space indent.
        std::string diff_line(char marker, int depth, const std::string& text) {
            return std::string(1, marker) + " " +
                   std::string(static_cast<size_t>(depth) * 2, ' ') + text;
        }
    } // namespace

    // TestCaseData is private to src/, so a public header cannot ask it
    // whether output is on. This crosses that boundary.
    bool diagnostics_visible(const TestCase& tc) {
        return tc.data()->should_log();
    }

    namespace {
        using Nodes = std::vector<ReprNode>;

        void render_parts(std::string& out, int depth, const Nodes& left,
                          const Nodes& right);

        // Descends into two composite parts, printing the opening they share
        // and then the difference of what they hold.
        void render_descent(std::string& out, int depth, const ReprNode& left,
                            const ReprNode& right, bool trailing_comma) {
            out += diff_line(' ', depth, left.label + left.prefix) + "\n";
            render_parts(out, depth + 1, left.parts, right.parts);
            out += diff_line(' ', depth, trailing_comma ? "}," : "}") + "\n";
        }

        // Two parts descend when they are shaped alike: same label, same
        // opening, and parts on both sides. Anything else has nothing to line
        // up inside, so it prints whole.
        bool descends(const ReprNode& left, const ReprNode& right) {
            return left.composite() && right.composite() &&
                   left.prefix == right.prefix && left.label == right.label;
        }

        // Prints the parts that are only on one side. A lone part on each
        // side is one part that changed, so it descends when it can.
        void render_run(std::string& out, int depth, const Nodes& left,
                        size_t left_begin, size_t left_end, const Nodes& right,
                        size_t right_begin, size_t right_end) {
            if (left_end - left_begin == 1 && right_end - right_begin == 1 &&
                descends(left[left_begin], right[right_begin])) {
                render_descent(out, depth, left[left_begin], right[right_begin],
                               /*trailing_comma=*/true);
                return;
            }
            for (size_t i = left_begin; i < left_end; i++) {
                out += diff_line('-', depth, left[i].full() + ",") + "\n";
            }
            for (size_t j = right_begin; j < right_end; j++) {
                out += diff_line('+', depth, right[j].full() + ",") + "\n";
            }
        }

        void render_parts(std::string& out, int depth, const Nodes& left,
                          const Nodes& right) {
            std::vector<std::string> left_keys;
            std::vector<std::string> right_keys;
            left_keys.reserve(left.size());
            right_keys.reserve(right.size());
            for (const ReprNode& node : left) {
                left_keys.push_back(node.full());
            }
            for (const ReprNode& node : right) {
                right_keys.push_back(node.full());
            }

            size_t i = 0;
            size_t j = 0;
            // Every part carries a trailing comma, including the last, so a
            // change to the last part marks one line rather than two.
            for (const impl::require::Match& match :
                 impl::require::common_subsequence(left_keys, right_keys)) {
                render_run(out, depth, left, i, match.left, right, j,
                           match.right);
                out +=
                    diff_line(' ', depth, left[match.left].full() + ",") + "\n";
                i = match.left + 1;
                j = match.right + 1;
            }
            render_run(out, depth, left, i, left.size(), right, j,
                       right.size());
        }
    } // namespace

    std::string render_node_diff(const ReprNode& left, const ReprNode& right) {
        if (!descends(left, right)) {
            return diff_line('-', 0, left.full()) + "\n" +
                   diff_line('+', 0, right.full());
        }
        std::string out;
        render_descent(out, 0, left, right, /*trailing_comma=*/false);
        out.pop_back(); // the report adds its own line break
        return out;
    }

    void fail_require(const char* origin, const std::string& message) {
        throw HegelRequireFailure(origin, message);
    }

} // namespace hegel::internal
