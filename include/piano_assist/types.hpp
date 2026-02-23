#pragma once

#include <optional>
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
    std::optional<int> bpm{};
};

struct AppSettings {
    bool strict_mode{true};
    int input_poll_interval_ms{8};
    OverlayChunkingMode overlay_chunking_mode{OverlayChunkingMode::AutoDetect};
    bool show_song_details{true};
    bool show_tag_details{true};
    bool show_bpm_details{true};
    bool show_sheet_tab_button{true};
    bool show_practice_sheet{false};
};

} // namespace piano_assist
