// SPDX-License-Identifier: MIT

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "piano_assist/song_repository.hpp"
#include "piano_assist/tag_store.hpp"

namespace {

class TempDirGuard final {
  public:
    TempDirGuard() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ =
            std::filesystem::temp_directory_path() / ("sheetmaster-tests-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }

    ~TempDirGuard() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_{};
};

std::string read_file_text(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

} // namespace

TEST_CASE("sanitize migration fixes missing names and legacy sustain tokens") {
    using piano_assist::MigrationSummary;
    using piano_assist::SongRepository;

    TempDirGuard temp;
    const std::filesystem::path sheets = temp.path() / "sheets";
    std::filesystem::create_directories(sheets);

    const std::filesystem::path song_file = sheets / "legacy_missing_name.PADATA";
    {
        std::ofstream out(song_file);
        REQUIRE(out.good());
        out << "#PA2_SONG_V1\n";
        out << "id=legacy_missing_name\n";
        out << "name=\n";
        out << "grouping=[]\n";
        out << "sustain=|\n";
        out << "---\n";
        out << "[tu]| a| b\n";
    }

    const SongRepository repository(sheets);
    const MigrationSummary preview = repository.preview_convention_migration();
    CHECK(preview.scanned_files == 1);
    CHECK(preview.files_to_rewrite == 1);
    CHECK(preview.missing_name == 1);
    CHECK(preview.normalized_sustain_metadata == 1);
    CHECK(preview.normalized_body_sustain_tokens == 1);

    const MigrationSummary applied = repository.apply_convention_migration();
    CHECK(applied.files_to_rewrite == 1);

    const std::string rewritten = read_file_text(song_file);
    CHECK(rewritten.find("name=legacy_missing_name") != std::string::npos);
    CHECK(rewritten.find("sustain=-") != std::string::npos);
    CHECK(rewritten.find('|') == std::string::npos);
}

TEST_CASE("sanitize tag repair fills missing tags and cleans stale keys") {
    using piano_assist::SongRepository;
    using piano_assist::TagRepairSummary;
    using piano_assist::TagStore;

    TempDirGuard temp;
    const std::filesystem::path sheets = temp.path() / "sheets";
    std::filesystem::create_directories(sheets);

    const SongRepository repository(sheets);
    const std::string first_id = repository.import_song("Song A", "a b c ", '[', ']', '-');
    const std::string second_id = repository.import_song("Song B", "d e f ", '[', ']', '-');
    const std::vector<piano_assist::Song> songs = repository.list_songs();
    REQUIRE(songs.size() == 2);

    const std::filesystem::path tag_file = sheets / "song_tags.PADISCRIM";
    {
        std::ofstream out(tag_file, std::ios::trunc);
        REQUIRE(out.good());
        out << first_id << "\t\n";
        out << "Song A\tLegacyAlt\n";
        out << "Song B\tCinematic\n";
        out << "ghost_entry\tLegacyTag\n";
    }

    const TagStore tag_store(tag_file);
    const TagRepairSummary preview = tag_store.preview_sanitize_repairs(songs, "Virtual Piano");
    CHECK(preview.songs_scanned == 2);
    CHECK(preview.songs_missing_tags == 1);
    CHECK(preview.songs_with_empty_tags == 1);
    CHECK(preview.legacy_name_keys_migrated == 1);
    CHECK(preview.duplicate_name_keys_removed == 1);
    CHECK(preview.orphan_tag_entries_removed == 1);

    const TagRepairSummary applied = tag_store.apply_sanitize_repairs(songs, "Virtual Piano");
    CHECK(applied.has_changes());

    const std::vector<std::string> first_tags = tag_store.tags_for_song(first_id);
    REQUIRE(first_tags.size() == 1);
    CHECK(first_tags.front() == "Virtual Piano");

    const std::vector<std::string> second_tags = tag_store.tags_for_song(second_id);
    REQUIRE(second_tags.size() == 1);
    CHECK(second_tags.front() == "Cinematic");

    const std::string tag_text = read_file_text(tag_file);
    CHECK(tag_text.find("Song A\tLegacyAlt") == std::string::npos);
    CHECK(tag_text.find("ghost_entry") == std::string::npos);
}
