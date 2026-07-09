#include <gtest/gtest.h>

#include <regex>
#include <stdexcept>
#include <string>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

// Draw every format/string generator end-to-end. The draw is the assertion: if
// libhegel rejects a generator's parameters it returns an error and the
// construction or draw throws, failing the property (and the test). The
// *distribution* of values is libhegel's concern; here we only confirm each
// parameterization is accepted.
TEST(Formats, DrawAll) {
    hegel::test(
        [](hegel::TestCase& tc) {
            // Standard format generators.
            (void)tc.draw(gs::emails());
            (void)tc.draw(gs::urls());
            (void)tc.draw(gs::domains());
            (void)tc.draw(gs::dates());
            (void)tc.draw(gs::times());
            (void)tc.draw(gs::datetimes());
            (void)tc.draw(gs::from_regex("[a-z]{1,5}"));
            (void)tc.draw(gs::from_regex("[A-Z]{2}", false));
            // ip_addresses: v4, v6, and the default (either) factory branches.
            (void)tc.draw(gs::ip_addresses({.v = 4}));
            (void)tc.draw(gs::ip_addresses({.v = 6}));
            (void)tc.draw(gs::ip_addresses());

            // characters() with each filtering field. Values are ones the
            // engine accepts; categories and exclude_categories are mutually
            // exclusive, so they go in separate draws.
            (void)tc.draw(gs::characters({.codec = "ascii",
                                          .min_codepoint = 97,
                                          .max_codepoint = 122,
                                          .exclude_characters = "aeiou"}));
            (void)tc.draw(gs::characters({.categories = {{"Nd"}}}));
            (void)tc.draw(gs::characters({.exclude_categories = {{"Cc"}}}));
            (void)tc.draw(gs::characters({.include_characters = "xyz"}));

            // text() routes the same fields through apply_char_fields, plus its
            // own size bounds and the dedicated alphabet branch.
            (void)tc.draw(
                gs::text({.min_size = 1, .max_size = 5, .codec = "ascii"}));
            (void)tc.draw(gs::text({.max_size = 5, .alphabet = "abc"}));

            // binary() with a max_size bound.
            (void)tc.draw(gs::binary({.max_size = 10}));
        },
        hegel::Settings{.test_cases = 50,
                        .database = hegel::Database::disabled()});
}

// alphabet cannot be combined with individual character-filtering options.
TEST(Formats, AlphabetWithCharFilteringThrows) {
    EXPECT_THROW(gs::text({.codec = "ascii", .alphabet = "abc"}),
                 std::invalid_argument);
}

// The engine validates string-generator parameters at construction; its
// rejections surface as std::invalid_argument.
TEST(Formats, InvalidRegexThrows) {
    EXPECT_THROW(gs::from_regex("("), std::invalid_argument);
}

TEST(Formats, UnknownCodecThrows) {
    EXPECT_THROW(gs::text({.codec = "no-such-codec"}), std::invalid_argument);
}

// A word-boundary anchor in a non-final position constrains the surrounding
// characters, so every generated string must contain a standalone "foo". Each
// draw is searched with the same pattern it came from, using std::regex as the
// oracle.
TEST(Formats, RegexNonFinalAnchorHonored) {
    const std::regex oracle(R"(\bfoo\b)");
    hegel::test(
        [&](hegel::TestCase& tc) {
            std::string s = tc.draw(gs::from_regex(R"(\bfoo\b)", false));
            EXPECT_TRUE(std::regex_search(s, oracle))
                << "generated string has no standalone \"foo\": " << s;
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
}

// By default the entire generated string must match the pattern, as if
// anchored with ^...$ — no surrounding characters are permitted.
TEST(Formats, RegexDefaultsToFullmatch) {
    const std::regex oracle("[A-Z]{2}-[0-9]{4}");
    hegel::test(
        [&](hegel::TestCase& tc) {
            std::string s = tc.draw(gs::from_regex("[A-Z]{2}-[0-9]{4}"));
            EXPECT_TRUE(std::regex_match(s, oracle))
                << "generated string is not a full match: " << s;
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
}

// fullmatch = false restores contains-a-match behavior: every generated
// string contains a match, but may carry arbitrary prefix/suffix characters.
// Padding around the match must actually occur across the run — otherwise this
// would pass even if the flag were ignored, since a full match also satisfies
// regex_search.
TEST(Formats, RegexFullmatchFalseAllowsContains) {
    const std::regex oracle("[A-Z]{2}-[0-9]{4}");
    bool saw_padding = false;
    hegel::test(
        [&](hegel::TestCase& tc) {
            std::string s = tc.draw(gs::from_regex("[A-Z]{2}-[0-9]{4}", false));
            EXPECT_TRUE(std::regex_search(s, oracle))
                << "generated string contains no match: " << s;
            saw_padding |= !std::regex_match(s, oracle);
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
    EXPECT_TRUE(saw_padding);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
