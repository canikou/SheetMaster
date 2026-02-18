#pragma once

#include <filesystem>

#include "piano_assist/types.hpp"

namespace piano_assist {

class SettingsStore final {
public:
    explicit SettingsStore(std::filesystem::path settings_file);

    [[nodiscard]] AppSettings load() const;
    void save(const AppSettings& settings) const;

private:
    std::filesystem::path settings_file_;
};

} // namespace piano_assist
