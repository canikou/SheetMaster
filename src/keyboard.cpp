#include "piano_assist/keyboard.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <unordered_set>

#include <windows.h>

namespace piano_assist {
namespace {

char normalize_key(const char value) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
}

std::array<int, 46> monitored_vk_codes() {
    std::array<int, 46> codes{};
    int index = 0;
    for (int vk = 0x30; vk <= 0x39; ++vk) {
        codes[static_cast<std::size_t>(index++)] = vk;
    }
    for (int vk = 0x41; vk <= 0x5A; ++vk) {
        codes[static_cast<std::size_t>(index++)] = vk;
    }

    // Common punctuation keys used by online piano/game sheets.
    constexpr std::array<int, 10> punctuation = {
        0xBD, // -
        0xBB, // =
        0xDB, // [
        0xDD, // ]
        0xDC, // backslash
        0xBA, // ;
        0xDE, // '
        0xBC, // ,
        0xBE, // .
        0xBF  // /
    };
    for (const int vk : punctuation) {
        codes[static_cast<std::size_t>(index++)] = vk;
    }

    return codes;
}

const std::array<int, 46>& get_monitored_vk_codes() {
    static const std::array<int, 46> codes = monitored_vk_codes();
    return codes;
}

} // namespace

KeyboardInput::KeyboardInput(const bool strict_mode) : strict_mode_(strict_mode) {}

void KeyboardInput::set_strict_mode(const bool strict_mode) {
    strict_mode_ = strict_mode;
}

bool KeyboardInput::check_chord(const std::string_view keys) const {
    std::unordered_set<int> required_vk_codes;
    required_vk_codes.reserve(keys.size());

    for (const char raw_key : keys) {
        if (raw_key == '-' || raw_key == '|' || std::isspace(static_cast<unsigned char>(raw_key))) {
            continue;
        }

        const char normalized = normalize_key(raw_key);
        const SHORT vk = VkKeyScanA(normalized);
        if (vk == -1) {
            return false;
        }
        const int vk_code = vk & 0xFF;
        required_vk_codes.insert(vk_code);
        if ((GetAsyncKeyState(vk_code) & 0x8000) == 0) {
            return false;
        }
    }

    if (required_vk_codes.empty()) {
        return false;
    }

    if (!strict_mode_) {
        return true;
    }

    for (const int vk : get_monitored_vk_codes()) {
        if ((GetAsyncKeyState(vk) & 0x8000) == 0) {
            continue;
        }

        if (required_vk_codes.find(vk) == required_vk_codes.end()) {
            return false;
        }
    }

    return true;
}

bool KeyboardInput::is_any_monitored_key_down() {
    for (const int vk : get_monitored_vk_codes()) {
        if (GetAsyncKeyState(vk) & 0x8000) {
            return true;
        }
    }
    return false;
}

void KeyboardInput::wait_for_any_release() {
    bool any_pressed = true;
    while (any_pressed) {
        any_pressed = false;
        for (const int vk : get_monitored_vk_codes()) {
            if (GetAsyncKeyState(vk) & 0x8000) {
                any_pressed = true;
                break;
            }
        }
        if (any_pressed) {
            Sleep(1);
        }
    }
}

} // namespace piano_assist
