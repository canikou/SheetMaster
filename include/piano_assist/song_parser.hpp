#pragma once

#include <string>
#include <vector>

#include "piano_assist/types.hpp"

namespace piano_assist {

std::vector<NoteGroup> parse_sheet(
    const std::string& raw,
    char open_brace,
    char close_brace,
    char sustain_indicator
);

} // namespace piano_assist
