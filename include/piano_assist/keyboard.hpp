#pragma once

#include <string_view>

namespace piano_assist {

class KeyboardInput final {
public:
    explicit KeyboardInput(bool strict_mode = true);

    void set_strict_mode(bool strict_mode);
    [[nodiscard]] bool check_chord(std::string_view keys) const;

    static void wait_for_any_release();
    [[nodiscard]] static bool is_any_monitored_key_down();

private:
    bool strict_mode_{true};
};

} // namespace piano_assist
