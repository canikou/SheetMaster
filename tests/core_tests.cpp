#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "piano_assist/song_parser.hpp"

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    using piano_assist::NoteGroup;
    using piano_assist::parse_sheet;

    const std::vector<NoteGroup> parsed = parse_sheet("[tf]- [rd]| a ", '[', ']', '-');
    expect(parsed.size() == 3, "expected three note groups");
    expect(parsed[0].keys == "tf-", "first group should include sustain marker");
    expect(parsed[1].keys == "rd|", "second group should keep alternate sustain marker");
    expect(parsed[2].keys == "a", "third group should be single note");

    return 0;
}
