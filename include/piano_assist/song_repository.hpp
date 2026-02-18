#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "piano_assist/types.hpp"

namespace piano_assist {

class SongRepository final {
public:
    explicit SongRepository(std::filesystem::path sheet_folder);

    void ensure_storage() const;
    [[nodiscard]] std::vector<Song> list_songs(std::string_view filter = {}) const;
    [[nodiscard]] std::vector<NoteGroup> load_sheet(const Song& song) const;
    [[nodiscard]] std::string load_raw_sheet_text(const Song& song) const;

    [[nodiscard]] std::string import_song(
        std::string_view requested_name,
        std::string_view raw_sheet_data,
        char open_brace,
        char close_brace,
        char sustain_indicator
    ) const;
    [[nodiscard]] std::string rename_song(const Song& song, std::string_view new_name) const;
    void delete_song(const Song& song) const;
    void update_song_contents(const Song& song, std::string_view raw_sheet_data) const;

private:
    std::filesystem::path sheet_folder_;
    mutable bool migration_checked_{false};

    [[nodiscard]] static std::string to_lower(std::string_view value);
    [[nodiscard]] static std::string normalize_display_name(std::string_view name);
    [[nodiscard]] std::filesystem::path make_unique_path(std::string_view base_id) const;
    void migrate_legacy_files_if_needed() const;
};

} // namespace piano_assist
