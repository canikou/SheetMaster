#include "piano_assist/tag_store.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>
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
        tag.erase(std::remove_if(tag.begin(), tag.end(), [](const unsigned char c) {
            return std::iscntrl(c) != 0 || c == '\t' || c == ',' || c == ';' || c == '|';
        }), tag.end());
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
        const std::string song_name = trim(delimiter == std::string::npos ? line : line.substr(0, delimiter));
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

        std::filesystem::copy_file(
            legacy_path,
            storage_file_,
            std::filesystem::copy_options::overwrite_existing,
            error
        );
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

void TagStore::set_tags_for_song(const std::string_view song_name, const std::vector<std::string>& tags) const {
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

void TagStore::rename_song(const std::string_view old_song_name, const std::string_view new_song_name) const {
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
