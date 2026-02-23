#include "piano_assist/keyboard.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace piano_assist {
namespace {

#ifdef _WIN32
char normalize_key(const char value) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
}

bool is_vk_down(const int vk_code) { return (GetAsyncKeyState(vk_code) & 0x8000) != 0; }

std::array<int, 46> monitored_vk_codes() {
    std::array<int, 46> codes{};
    int index = 0;
    for (int vk_code = 0x30; vk_code <= 0x39; ++vk_code) {
        codes[static_cast<std::size_t>(index++)] = vk_code;
    }
    for (int vk_code = 0x41; vk_code <= 0x5A; ++vk_code) {
        codes[static_cast<std::size_t>(index++)] = vk_code;
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
#endif

} // namespace

KeyboardInput::KeyboardInput(const bool strict_mode) : strict_mode_(strict_mode) {}

void KeyboardInput::set_strict_mode(const bool strict_mode) { strict_mode_ = strict_mode; }

bool KeyboardInput::check_chord(const std::string_view keys) const {
#ifdef _WIN32
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
        if (!is_vk_down(vk_code)) {
            return false;
        }
    }

    if (required_vk_codes.empty()) {
        return false;
    }

    if (!strict_mode_) {
        return true;
    }

    for (const int vk_code : get_monitored_vk_codes()) {
        if (!is_vk_down(vk_code)) {
            continue;
        }

        if (!required_vk_codes.contains(vk_code)) {
            return false;
        }
    }

    return true;
#else
    (void)keys;
    return false;
#endif
}

bool KeyboardInput::is_any_monitored_key_down() {
#ifdef _WIN32
    for (const int vk_code : get_monitored_vk_codes()) {
        if (is_vk_down(vk_code)) {
            return true;
        }
    }
    return false;
#else
    return false;
#endif
}

void KeyboardInput::wait_for_any_release() {
#ifdef _WIN32
    bool any_pressed = true;
    while (any_pressed) {
        any_pressed = false;
        for (const int vk_code : get_monitored_vk_codes()) {
            if (is_vk_down(vk_code)) {
                any_pressed = true;
                break;
            }
        }
        if (any_pressed) {
            Sleep(1);
        }
    }
#endif
}

} // namespace piano_assist
