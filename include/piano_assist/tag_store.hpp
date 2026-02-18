#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "piano_assist/types.hpp"

namespace piano_assist {

class TagStore final {
public:
    explicit TagStore(std::filesystem::path storage_file);

    void migrate_song_name_keys_to_ids(const std::vector<Song>& songs) const;

    [[nodiscard]] std::vector<std::string> tags_for_song(std::string_view song_name) const;
    [[nodiscard]] std::vector<std::string> list_all_tags() const;

    void set_tags_for_song(std::string_view song_name, const std::vector<std::string>& tags) const;
    void remove_song(std::string_view song_name) const;
    void rename_song(std::string_view old_song_name, std::string_view new_song_name) const;

private:
    std::filesystem::path storage_file_;
};

} // namespace piano_assist
