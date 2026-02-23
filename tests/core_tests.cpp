#include <doctest/doctest.h>

#include <vector>

#include "piano_assist/song_parser.hpp"

TEST_CASE("parse_sheet keeps grouped chords and sustain markers") {
    using piano_assist::NoteGroup;
    using piano_assist::parse_sheet;

    const std::vector<NoteGroup> parsed = parse_sheet("[tf]- [rd]| a ", '[', ']', '-');
    REQUIRE(parsed.size() == 3);
    CHECK(parsed[0].keys == "tf-");
    CHECK(parsed[1].keys == "rd|");
    CHECK(parsed[2].keys == "a");
}
