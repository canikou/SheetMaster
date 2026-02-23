#include <doctest/doctest.h>

#include <vector>

#include "piano_assist/song_parser.hpp"

TEST_CASE("parse_sheet supports alternate grouping and sustain token") {
    using piano_assist::NoteGroup;
    using piano_assist::parse_sheet;

    const std::vector<NoteGroup> parsed = parse_sheet("(ab)| c (de)-", '(', ')', '|');
    REQUIRE(parsed.size() == 3);
    CHECK(parsed[0].keys == "ab|");
    CHECK(parsed[1].keys == "c");
    CHECK(parsed[2].keys == "de-");
}
