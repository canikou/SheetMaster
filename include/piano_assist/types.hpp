#pragma once

#include <string>

namespace piano_assist {

enum class OverlayChunkingMode {
    AutoDetect = 0,
    Smart = 1,
};

struct NoteGroup {
    std::string keys{};
    bool was_correct{true};
};

struct Song {
    std::string id{};
    std::string name{};
    std::string file_name{};
    char open_brace{'['};
    char close_brace{']'};
    char sustain_indicator{'-'};
};

struct AppSettings {
    bool strict_mode{true};
    int input_poll_interval_ms{8};
    OverlayChunkingMode overlay_chunking_mode{OverlayChunkingMode::AutoDetect};
};

} // namespace piano_assist
