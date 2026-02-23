#include "piano_assist/tag_store.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace piano_assist {
namespace {

using TagMap = std::unordered_map<std::string, std::vector<std::string>>;

std::string trim(std::string_view value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

std::vector<std::string> normalize_tags(std::vector<std::string> tags) {
    std::set<std::string> unique;
    std::vector<std::string> normalized;

    for (std::string& tag : tags) {
        tag = trim(tag);
        tag.erase(std::remove_if(tag.begin(), tag.end(),
                                 [](const unsigned char c) {
                                     return std::iscntrl(c) != 0 || c == '\t' || c == ',' ||
                                            c == ';' || c == '|';
                                 }),
                  tag.end());
        if (tag.empty()) {
            continue;
        }
        if (unique.insert(tag).second) {
            normalized.push_back(std::move(tag));
        }
    }

    return normalized;
}

std::vector<std::filesystem::path> legacy_tag_paths_for(const std::filesystem::path& modern_path) {
    std::vector<std::filesystem::path> candidates;

    std::filesystem::path same_folder_legacy = modern_path;
    same_folder_legacy.replace_extension(".txt");
    candidates.push_back(same_folder_legacy);

    const std::filesystem::path parent = modern_path.parent_path();
    if (!parent.empty()) {
        const std::filesystem::path old_root_legacy = parent.parent_path() / "song_tags.txt";
        if (old_root_legacy != same_folder_legacy) {
            candidates.push_back(old_root_legacy);
        }
    }

    return candidates;
}

TagMap load_map(const std::filesystem::path& storage_file) {
    TagMap result;
    std::ifstream in(storage_file);
    if (!in) {
        for (const std::filesystem::path& legacy_path : legacy_tag_paths_for(storage_file)) {
            in.clear();
            in.open(legacy_path);
            if (in) {
                break;
            }
        }
        if (!in) {
            return result;
        }
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }

        const std::size_t delimiter = line.find('\t');
        const std::string song_name =
            trim(delimiter == std::string::npos ? line : line.substr(0, delimiter));
        if (song_name.empty()) {
            continue;
        }

        std::vector<std::string> tags;
        if (delimiter != std::string::npos) {
            std::istringstream values(line.substr(delimiter + 1));
            std::string item;
            while (std::getline(values, item, ',')) {
                tags.push_back(item);
            }
        }

        result[song_name] = normalize_tags(std::move(tags));
    }

    return result;
}

void save_map(const std::filesystem::path& storage_file, const TagMap& map) {
    std::error_code error;
    if (storage_file.has_parent_path()) {
        std::filesystem::create_directories(storage_file.parent_path(), error);
    }

    std::ofstream out(storage_file, std::ios::trunc);
    if (!out) {
        return;
    }

    std::vector<std::string> song_names;
    song_names.reserve(map.size());
    for (const auto& entry : map) {
        song_names.push_back(entry.first);
    }
    std::sort(song_names.begin(), song_names.end());

    for (const std::string& song_name : song_names) {
        const auto it = map.find(song_name);
        if (it == map.end()) {
            continue;
        }

        out << song_name << '\t';
        const std::vector<std::string> tags = normalize_tags(it->second);
        for (std::size_t index = 0; index < tags.size(); ++index) {
            if (index > 0) {
                out << ',';
            }
            out << tags[index];
        }
        out << '\n';
    }
}

void append_sample(TagRepairSummary* summary, std::string value) {
    if (summary == nullptr) {
        return;
    }

    value = trim(value);
    if (value.empty()) {
        return;
    }

    const auto already_recorded = std::find(summary->sample_affected_songs.begin(),
                                            summary->sample_affected_songs.end(), value);
    if (already_recorded != summary->sample_affected_songs.end()) {
        return;
    }

    if (summary->sample_affected_songs.size() >= 5) {
        return;
    }
    summary->sample_affected_songs.push_back(std::move(value));
}

TagRepairSummary inspect_or_apply_tag_repairs(const std::filesystem::path& storage_file,
                                              const std::vector<Song>& songs,
                                              const std::string_view default_tag,
                                              const bool apply_changes) {
    TagRepairSummary summary{};
    summary.songs_scanned = songs.size();
    if (songs.empty()) {
        return summary;
    }

    std::vector<std::string> normalized_default_tags = normalize_tags({trim(default_tag)});
    if (normalized_default_tags.empty()) {
        return summary;
    }

    TagMap map = load_map(storage_file);
    bool changed = false;

    std::unordered_map<std::string, int> name_counts;
    name_counts.reserve(songs.size());
    std::unordered_map<std::string, std::string> unique_name_to_id;
    unique_name_to_id.reserve(songs.size());
    std::unordered_set<std::string> song_ids;
    song_ids.reserve(songs.size());
    std::unordered_set<std::string> song_names;
    song_names.reserve(songs.size());

    for (const Song& song : songs) {
        const std::string id_key = trim(song.id);
        const std::string name_key = trim(song.name);
        if (!id_key.empty()) {
            song_ids.insert(id_key);
        }
        if (!name_key.empty()) {
            song_names.insert(name_key);
            ++name_counts[name_key];
        }
    }

    for (const Song& song : songs) {
        const std::string id_key = trim(song.id);
        const std::string name_key = trim(song.name);
        if (id_key.empty() || name_key.empty()) {
            continue;
        }

        const auto name_count = name_counts.find(name_key);
        if (name_count != name_counts.end() && name_count->second == 1) {
            unique_name_to_id[name_key] = id_key;
        }
    }

    for (const Song& song : songs) {
        const std::string id_key = trim(song.id);
        const std::string name_key = trim(song.name);
        if (id_key.empty()) {
            continue;
        }

        auto id_it = map.find(id_key);
        if (id_it == map.end() && !name_key.empty()) {
            const auto name_count = name_counts.find(name_key);
            if (name_count != name_counts.end() && name_count->second == 1) {
                const auto name_it = map.find(name_key);
                if (name_it != map.end()) {
                    map[id_key] = normalize_tags(name_it->second);
                    map.erase(name_it);
                    id_it = map.find(id_key);
                    ++summary.legacy_name_keys_migrated;
                    append_sample(&summary, song.name);
                    changed = true;
                }
            }
        }

        if (id_it == map.end()) {
            ++summary.songs_missing_tags;
            append_sample(&summary, song.name);
            if (apply_changes) {
                map[id_key] = normalized_default_tags;
                changed = true;
            }
            continue;
        }

        const std::vector<std::string> normalized = normalize_tags(id_it->second);
        if (normalized.empty()) {
            ++summary.songs_missing_tags;
            ++summary.songs_with_empty_tags;
            append_sample(&summary, song.name);
            if (apply_changes) {
                map[id_key] = normalized_default_tags;
                changed = true;
            }
            continue;
        }

        if (normalized != id_it->second) {
            map[id_key] = normalized;
            changed = true;
        }
    }

    for (const auto& [name_key, id_key] : unique_name_to_id) {
        const auto name_it = map.find(name_key);
        const auto id_it = map.find(id_key);
        if (name_it == map.end() || id_it == map.end()) {
            continue;
        }

        ++summary.duplicate_name_keys_removed;
        append_sample(&summary, name_key);
        if (apply_changes) {
            map.erase(name_it);
            changed = true;
        }
    }

    for (auto it = map.begin(); it != map.end();) {
        const std::string key = trim(it->first);
        if (!key.empty() && (song_ids.contains(key) || song_names.contains(key))) {
            ++it;
            continue;
        }

        ++summary.orphan_tag_entries_removed;
        append_sample(&summary, key);
        if (apply_changes) {
            it = map.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    if (apply_changes && changed) {
        save_map(storage_file, map);
    }

    return summary;
}

} // namespace

TagStore::TagStore(std::filesystem::path storage_file) : storage_file_(std::move(storage_file)) {
    std::error_code error;
    if (storage_file_.has_parent_path()) {
        std::filesystem::create_directories(storage_file_.parent_path(), error);
    }

    if (std::filesystem::exists(storage_file_)) {
        return;
    }

    for (const std::filesystem::path& legacy_path : legacy_tag_paths_for(storage_file_)) {
        if (!std::filesystem::exists(legacy_path)) {
            continue;
        }

        std::filesystem::copy_file(legacy_path, storage_file_,
                                   std::filesystem::copy_options::overwrite_existing, error);
        if (!error) {
            std::filesystem::remove(legacy_path, error);
            break;
        }
        error.clear();
    }
}

void TagStore::migrate_song_name_keys_to_ids(const std::vector<Song>& songs) const {
    TagMap map = load_map(storage_file_);
    if (map.empty() || songs.empty()) {
        return;
    }

    std::unordered_map<std::string, int> name_counts;
    name_counts.reserve(songs.size());
    for (const Song& song : songs) {
        ++name_counts[trim(song.name)];
    }

    bool changed = false;
    for (const Song& song : songs) {
        const std::string id_key = trim(song.id);
        const std::string name_key = trim(song.name);
        if (id_key.empty() || name_key.empty() || id_key == name_key) {
            continue;
        }

        const auto existing_id = map.find(id_key);
        if (existing_id != map.end()) {
            continue;
        }

        const auto name_count = name_counts.find(name_key);
        if (name_count == name_counts.end() || name_count->second != 1) {
            continue;
        }

        const auto existing_name = map.find(name_key);
        if (existing_name == map.end()) {
            continue;
        }

        map[id_key] = normalize_tags(existing_name->second);
        map.erase(existing_name);
        changed = true;
    }

    if (changed) {
        save_map(storage_file_, map);
    }
}

void TagStore::ensure_default_tag_for_songs(const std::vector<Song>& songs,
                                            const std::string_view default_tag) const {
    if (songs.empty()) {
        return;
    }

    const std::string normalized_default = trim(default_tag);
    if (normalized_default.empty()) {
        return;
    }

    TagMap map = load_map(storage_file_);
    bool changed = false;

    for (const Song& song : songs) {
        const std::string key = trim(song.id);
        if (key.empty()) {
            continue;
        }

        const auto it = map.find(key);
        if (it == map.end()) {
            map[key] = {normalized_default};
            changed = true;
            continue;
        }

        const std::vector<std::string> normalized_tags = normalize_tags(it->second);
        if (normalized_tags.empty()) {
            map[key] = {normalized_default};
            changed = true;
            continue;
        }

        if (normalized_tags != it->second) {
            map[key] = normalized_tags;
            changed = true;
        }
    }

    if (changed) {
        save_map(storage_file_, map);
    }
}

TagRepairSummary TagStore::preview_sanitize_repairs(const std::vector<Song>& songs,
                                                    const std::string_view default_tag) const {
    return inspect_or_apply_tag_repairs(storage_file_, songs, default_tag, false);
}

TagRepairSummary TagStore::apply_sanitize_repairs(const std::vector<Song>& songs,
                                                  const std::string_view default_tag) const {
    return inspect_or_apply_tag_repairs(storage_file_, songs, default_tag, true);
}

std::vector<std::string> TagStore::tags_for_song(const std::string_view song_name) const {
    const TagMap map = load_map(storage_file_);
    const auto it = map.find(std::string(song_name));
    if (it == map.end()) {
        return {};
    }
    return normalize_tags(it->second);
}

std::vector<std::string> TagStore::list_all_tags() const {
    const TagMap map = load_map(storage_file_);
    std::set<std::string> unique;
    for (const auto& entry : map) {
        for (const std::string& tag : entry.second) {
            unique.insert(tag);
        }
    }
    return std::vector<std::string>(unique.begin(), unique.end());
}

void TagStore::set_tags_for_song(const std::string_view song_name,
                                 const std::vector<std::string>& tags) const {
    const std::string key = trim(song_name);
    if (key.empty()) {
        return;
    }

    TagMap map = load_map(storage_file_);
    map[key] = normalize_tags(tags);
    save_map(storage_file_, map);
}

void TagStore::remove_song(const std::string_view song_name) const {
    TagMap map = load_map(storage_file_);
    map.erase(trim(song_name));
    save_map(storage_file_, map);
}

void TagStore::rename_song(const std::string_view old_song_name,
                           const std::string_view new_song_name) const {
    const std::string old_key = trim(old_song_name);
    const std::string new_key = trim(new_song_name);
    if (old_key.empty() || new_key.empty() || old_key == new_key) {
        return;
    }

    TagMap map = load_map(storage_file_);
    const auto it = map.find(old_key);
    if (it == map.end()) {
        return;
    }

    map[new_key] = normalize_tags(it->second);
    map.erase(it);
    save_map(storage_file_, map);
}

} // namespace piano_assist
