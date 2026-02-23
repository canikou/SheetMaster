#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "piano_assist/types.hpp"

namespace piano_assist {

struct TagRepairSummary {
    std::size_t songs_scanned{0};
    std::size_t songs_missing_tags{0};
    std::size_t songs_with_empty_tags{0};
    std::size_t legacy_name_keys_migrated{0};
    std::size_t duplicate_name_keys_removed{0};
    std::size_t orphan_tag_entries_removed{0};
    std::vector<std::string> sample_affected_songs{};

    [[nodiscard]] bool has_changes() const {
        return songs_missing_tags > 0 || songs_with_empty_tags > 0 ||
               legacy_name_keys_migrated > 0 || duplicate_name_keys_removed > 0 ||
               orphan_tag_entries_removed > 0;
    }
};

class TagStore final {
  public:
    explicit TagStore(std::filesystem::path storage_file);

    void migrate_song_name_keys_to_ids(const std::vector<Song>& songs) const;
    void ensure_default_tag_for_songs(const std::vector<Song>& songs,
                                      std::string_view default_tag) const;
    [[nodiscard]] TagRepairSummary preview_sanitize_repairs(const std::vector<Song>& songs,
                                                            std::string_view default_tag) const;
    [[nodiscard]] TagRepairSummary apply_sanitize_repairs(const std::vector<Song>& songs,
                                                          std::string_view default_tag) const;

    [[nodiscard]] std::vector<std::string> tags_for_song(std::string_view song_name) const;
    [[nodiscard]] std::vector<std::string> list_all_tags() const;

    void set_tags_for_song(std::string_view song_name, const std::vector<std::string>& tags) const;
    void remove_song(std::string_view song_name) const;
    void rename_song(std::string_view old_song_name, std::string_view new_song_name) const;

  private:
    std::filesystem::path storage_file_;
};

} // namespace piano_assist
