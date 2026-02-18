#include "piano_assist/settings_store.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <sstream>
#include <utility>

namespace piano_assist {
namespace {

std::string trim(std::string value) {
    const auto begin = std::find_if(value.begin(), value.end(), [](const unsigned char ch) {
        return std::isspace(ch) == 0;
    });
    const auto end = std::find_if(value.rbegin(), value.rend(), [](const unsigned char ch) {
        return std::isspace(ch) == 0;
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

OverlayChunkingMode parse_chunking_mode(const std::string_view value) {
    const std::string normalized = trim(std::string(value));
    if (normalized == "smart" || normalized == "Smart") {
        return OverlayChunkingMode::Smart;
    }
    return OverlayChunkingMode::AutoDetect;
}

std::string chunking_mode_to_string(const OverlayChunkingMode mode) {
    return mode == OverlayChunkingMode::Smart ? "smart" : "auto_detect";
}

std::filesystem::path legacy_settings_path_for(const std::filesystem::path& modern_path) {
    std::filesystem::path legacy_path = modern_path;
    legacy_path.replace_extension(".txt");
    return legacy_path;
}

} // namespace

SettingsStore::SettingsStore(std::filesystem::path settings_file) : settings_file_(std::move(settings_file)) {
    std::error_code error;
    if (settings_file_.has_parent_path()) {
        std::filesystem::create_directories(settings_file_.parent_path(), error);
    }

    if (std::filesystem::exists(settings_file_)) {
        return;
    }

    const std::filesystem::path legacy_path = legacy_settings_path_for(settings_file_);
    if (!std::filesystem::exists(legacy_path)) {
        return;
    }

    std::filesystem::copy_file(
        legacy_path,
        settings_file_,
        std::filesystem::copy_options::overwrite_existing,
        error
    );
    if (!error) {
        std::filesystem::remove(legacy_path, error);
    }
}

AppSettings SettingsStore::load() const {
    AppSettings settings{};
    std::ifstream in(settings_file_);
    if (!in) {
        const std::filesystem::path legacy_path = legacy_settings_path_for(settings_file_);
        in.clear();
        in.open(legacy_path);
        if (!in) {
            return settings;
        }
    }

    std::string content(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );

    if (content.find('=') != std::string::npos) {
        std::istringstream lines(content);
        std::string line;
        while (std::getline(lines, line)) {
            const std::size_t delimiter = line.find('=');
            if (delimiter == std::string::npos) {
                continue;
            }

            const std::string key = trim(line.substr(0, delimiter));
            const std::string value = trim(line.substr(delimiter + 1));

            if (key == "strict_mode") {
                if (value == "true" || value == "1") {
                    settings.strict_mode = true;
                } else if (value == "false" || value == "0") {
                    settings.strict_mode = false;
                }
            } else if (key == "input_poll_interval_ms") {
                try {
                    const int parsed = std::stoi(value);
                    settings.input_poll_interval_ms = std::clamp(parsed, 1, 100);
                } catch (const std::exception&) {
                }
            } else if (key == "overlay_chunking_mode") {
                settings.overlay_chunking_mode = parse_chunking_mode(value);
            }
        }
        return settings;
    }

    std::istringstream legacy(content);
    legacy >> std::boolalpha >> settings.strict_mode;
    if (legacy.good() || legacy.eof()) {
        return settings;
    }

    legacy.clear();
    legacy.seekg(0);

    int legacy_value = 1;
    if (legacy >> legacy_value) {
        settings.strict_mode = legacy_value != 0;
    }
    return settings;
}

void SettingsStore::save(const AppSettings& settings) const {
    std::error_code error;
    if (settings_file_.has_parent_path()) {
        std::filesystem::create_directories(settings_file_.parent_path(), error);
    }

    std::ofstream out(settings_file_, std::ios::trunc);
    if (!out) {
        return;
    }

    out << std::boolalpha;
    out << "strict_mode=" << settings.strict_mode << '\n';
    out << "input_poll_interval_ms=" << std::clamp(settings.input_poll_interval_ms, 1, 100) << '\n';
    out << "overlay_chunking_mode=" << chunking_mode_to_string(settings.overlay_chunking_mode) << '\n';

    const std::filesystem::path legacy_path = legacy_settings_path_for(settings_file_);
    if (legacy_path != settings_file_ && std::filesystem::exists(legacy_path)) {
        std::filesystem::remove(legacy_path, error);
    }
}

} // namespace piano_assist
