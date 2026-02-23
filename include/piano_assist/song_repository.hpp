#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "piano_assist/types.hpp"

namespace piano_assist {

struct MigrationSummary {
    std::size_t scanned_files{0};
    std::size_t files_to_rewrite{0};
    std::size_t legacy_extension_files{0};
    std::size_t legacy_format_files{0};
    std::size_t missing_id{0};
    std::size_t missing_name{0};
    std::size_t normalized_grouping{0};
    std::size_t normalized_sustain_metadata{0};
    std::size_t normalized_body_sustain_tokens{0};
    std::vector<std::string> sample_changed_files{};

    [[nodiscard]] bool has_changes() const { return files_to_rewrite > 0; }
};

class SongRepository final {
  public:
    explicit SongRepository(std::filesystem::path sheet_folder);

    void ensure_storage() const;
    [[nodiscard]] std::vector<Song> list_songs(std::string_view filter = {}) const;
    [[nodiscard]] std::vector<NoteGroup> load_sheet(const Song& song) const;
    [[nodiscard]] std::string load_raw_sheet_text(const Song& song) const;

    [[nodiscard]] std::string import_song(std::string_view requested_name,
                                          std::string_view raw_sheet_data, char open_brace,
                                          char close_brace, char sustain_indicator,
                                          std::optional<int> bpm = std::nullopt) const;
    [[nodiscard]] std::string rename_song(const Song& song, std::string_view new_name) const;
    void delete_song(const Song& song) const;
    void update_song_contents(const Song& song, std::string_view raw_sheet_data) const;
    [[nodiscard]] MigrationSummary preview_convention_migration() const;
    [[nodiscard]] MigrationSummary apply_convention_migration() const;
    [[nodiscard]] const std::filesystem::path& sheet_folder() const { return sheet_folder_; }

  private:
    std::filesystem::path sheet_folder_;
    mutable bool migration_checked_{false};

    [[nodiscard]] static std::string to_lower(std::string_view value);
    [[nodiscard]] static std::string normalize_display_name(std::string_view name);
    [[nodiscard]] std::filesystem::path make_unique_path(std::string_view base_id) const;
    void migrate_legacy_files_if_needed() const;
};

} // namespace piano_assist
