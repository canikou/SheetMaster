#include "piano_assist/song_parser.hpp"

#include <cctype>

namespace piano_assist {

std::vector<NoteGroup> parse_sheet(
    const std::string& raw,
    const char open_brace,
    const char close_brace,
    const char sustain_indicator
) {
    std::vector<NoteGroup> sheet;
    std::string current_keys;
    bool in_brackets = false;

    const auto flush_current = [&sheet, &current_keys]() {
        if (!current_keys.empty()) {
            sheet.push_back(NoteGroup{current_keys, true});
            current_keys.clear();
        }
    };

    for (const char ch : raw) {
        if (ch == open_brace) {
            if (!in_brackets) {
                flush_current();
            }
            in_brackets = true;
            continue;
        }

        if (ch == close_brace) {
            flush_current();
            in_brackets = false;
            continue;
        }

        if (in_brackets) {
            if (std::isspace(static_cast<unsigned char>(ch)) == 0) {
                current_keys.push_back(ch);
            }
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch))) {
            flush_current();
            continue;
        }

        const bool is_sustain = (ch == sustain_indicator) || (ch == '-') || (ch == '|');
        if (is_sustain) {
            if (!sheet.empty()) {
                sheet.back().keys.push_back(ch);
            } else if (!current_keys.empty()) {
                current_keys.push_back(ch);
            }
            continue;
        }

        flush_current();
        current_keys.assign(1, ch);
    }

    flush_current();
    return sheet;
}

} // namespace piano_assist
