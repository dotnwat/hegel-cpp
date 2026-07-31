#pragma once

#include <regex>
#include <string>

#include <ApprovalTests.hpp>

// Scrubbers shared by the snapshot suites. Both hide the parts of a failure
// report that come from the environment rather than from the code under test.

namespace hegel::tests::common {

    /// @brief Hides a report's reproduction blob.
    ///
    /// A blob encodes engine internals and changes with the engine version, so
    /// the snapshots pin the report layout around a placeholder instead of the
    /// payload.
    inline ApprovalTests::Options scrub_blob() {
        return ApprovalTests::Options(
            ApprovalTests::Scrubbers::createRegexScrubber(
                std::regex(R"("[A-Za-z0-9+/=]{8,}")"), R"("[blob]")"));
    }

    /// @brief Hides a report's reproduction blob and the source position in
    /// its header.
    ///
    /// A header that hegel::test() fills in names the file the compiler was
    /// given, which is an absolute path, and the line of the call, which moves
    /// whenever the file above it does. Use this for a report Hegel named
    /// itself; a report named by an explicit TestLocation holds a fixed
    /// position worth pinning, so scrub only its blob.
    inline ApprovalTests::Options scrub_report() {
        return ApprovalTests::Options([](const std::string& text) {
            std::string blobless = std::regex_replace(
                text, std::regex(R"("[A-Za-z0-9+/=]{8,}")"), R"("[blob]")");
            return std::regex_replace(
                blobless, std::regex(R"((--- Failure: [^(\n]+\()[^\n]*)"),
                "$1[location]) ---");
        });
    }

} // namespace hegel::tests::common
