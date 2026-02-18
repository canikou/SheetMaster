#include "piano_assist/song_repository.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "piano_assist/song_parser.hpp"

namespace piano_assist {
namespace {

constexpr std::string_view kModernMarker = "#PA2_SONG_V1";
constexpr std::string_view kMetadataSeparator = "---";
constexpr std::string_view kSongDataExtension = ".PADATA";
constexpr std::string_view kSongDataExtensionLower = ".padata";
constexpr std::string_view kLegacySongDataExtensionLower = ".txt";
constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

struct SongDocument {
    std::string id{};
    std::string display_name{};
    char open_brace{'['};
    char close_brace{']'};
    char sustain_indicator{'-'};
    std::string body{};
    bool is_modern{false};
};

std::string trim(const std::string_view value) {
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

bool is_grouping_pair_valid(const char open_brace, const char close_brace) {
    return (open_brace == '[' && close_brace == ']') || (open_brace == '(' && close_brace == ')');
}

std::string grouping_token(const char open_brace, const char close_brace) {
    if (open_brace == '(' && close_brace == ')') {
        return "()";
    }
    return "[]";
}

std::pair<char, char> parse_grouping_token(const std::string_view token) {
    if (token == "()") {
        return {'(', ')'};
    }
    return {'[', ']'};
}

char sanitize_sustain_indicator(const char value) {
    return (value == '|') ? '|' : '-';
}

std::string normalize_display_name_value(const std::string_view name) {
    std::string cleaned;
    cleaned.reserve(name.size());
    for (const char c : name) {
        if (std::iscntrl(static_cast<unsigned char>(c)) == 0) {
            cleaned.push_back(c);
        }
    }

    cleaned = trim(cleaned);
    if (cleaned.empty()) {
        return "Untitled Song";
    }
    return cleaned;
}

std::string sanitize_song_id(const std::string_view value) {
    std::string cleaned;
    cleaned.reserve(value.size());

    for (const char raw : value) {
        const unsigned char c = static_cast<unsigned char>(raw);
        if (std::isalnum(c) != 0) {
            cleaned.push_back(static_cast<char>(std::tolower(c)));
        } else if (raw == '_' || raw == '-') {
            cleaned.push_back(raw);
        } else if (std::isspace(c) != 0) {
            cleaned.push_back('_');
        }
    }

    std::string compacted;
    compacted.reserve(cleaned.size());
    bool last_was_separator = true;
    for (const char c : cleaned) {
        const bool is_separator = c == '_' || c == '-';
        if (is_separator && last_was_separator) {
            continue;
        }
        compacted.push_back(c);
        last_was_separator = is_separator;
    }

    while (!compacted.empty() && (compacted.front() == '_' || compacted.front() == '-')) {
        compacted.erase(compacted.begin());
    }
    while (!compacted.empty() && (compacted.back() == '_' || compacted.back() == '-')) {
        compacted.pop_back();
    }

    if (compacted.empty()) {
        return "song";
    }
    return compacted;
}

std::string id_slug_from_name(const std::string_view name) {
    std::string slug = sanitize_song_id(normalize_display_name_value(name));
    if (slug.size() > 24) {
        slug.resize(24);
        while (!slug.empty() && (slug.back() == '_' || slug.back() == '-')) {
            slug.pop_back();
        }
    }
    if (slug.empty()) {
        return "song";
    }
    return slug;
}

std::string hex_u64(std::uint64_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string output(16, '0');
    for (int index = 15; index >= 0; --index) {
        output[static_cast<std::size_t>(index)] = digits[value & 0x0F];
        value >>= 4U;
    }
    return output;
}

std::uint64_t fnv1a_64(const std::string_view data) {
    std::uint64_t hash = kFnvOffsetBasis;
    for (const char raw : data) {
        hash ^= static_cast<unsigned char>(raw);
        hash *= kFnvPrime;
    }
    return hash;
}

std::string build_song_id(
    const std::string_view display_name,
    const std::string_view raw_sheet_data,
    const char open_brace,
    const char close_brace,
    const char sustain_indicator
) {
    std::string seed;
    seed.reserve(display_name.size() + raw_sheet_data.size() + 16);
    seed.append(normalize_display_name_value(display_name));
    seed.push_back('\n');
    seed.append(raw_sheet_data);
    seed.push_back('\n');
    seed.push_back(open_brace);
    seed.push_back(close_brace);
    seed.push_back(sustain_indicator);

    return id_slug_from_name(display_name) + "_" + hex_u64(fnv1a_64(seed));
}

SongDocument read_song_document(const std::filesystem::path& path) {
    SongDocument document{};

    std::ifstream in(path);
    if (!in) {
        return document;
    }

    std::string first_line;
    if (!std::getline(in, first_line)) {
        return document;
    }

    const std::string first_trimmed = trim(first_line);
    if (first_trimmed == kModernMarker) {
        document.is_modern = true;
        std::string line;
        while (std::getline(in, line)) {
            if (trim(line) == kMetadataSeparator) {
                break;
            }

            const std::size_t delimiter = line.find('=');
            if (delimiter == std::string::npos) {
                continue;
            }

            const std::string key = trim(line.substr(0, delimiter));
            const std::string value = trim(line.substr(delimiter + 1));
            if (key == "id") {
                document.id = sanitize_song_id(value);
            } else if (key == "name") {
                document.display_name = normalize_display_name_value(value);
            } else if (key == "grouping") {
                const auto [open_brace, close_brace] = parse_grouping_token(value);
                document.open_brace = open_brace;
                document.close_brace = close_brace;
            } else if (key == "sustain") {
                if (!value.empty()) {
                    document.sustain_indicator = sanitize_sustain_indicator(value.front());
                }
            }
        }

        std::ostringstream body;
        bool first = true;
        while (std::getline(in, line)) {
            if (!first) {
                body << '\n';
            }
            body << line;
            first = false;
        }
        document.body = body.str();
        return document;
    }

    const auto [legacy_open, legacy_close] = parse_grouping_token(first_trimmed);
    if (first_trimmed == "[]" || first_trimmed == "()") {
        document.open_brace = legacy_open;
        document.close_brace = legacy_close;
    } else {
        std::ostringstream body;
        body << first_line;
        std::string line;
        while (std::getline(in, line)) {
            body << '\n' << line;
        }
        document.body = body.str();
        return document;
    }

    std::ostringstream body;
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (!first) {
            body << '\n';
        }
        body << line;
        first = false;
    }
    document.body = body.str();
    return document;
}

void write_song_document(const std::filesystem::path& path, const SongDocument& document) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Unable to write song file.");
    }

    const char open_brace = is_grouping_pair_valid(document.open_brace, document.close_brace)
                                ? document.open_brace
                                : '[';
    const char close_brace = is_grouping_pair_valid(document.open_brace, document.close_brace)
                                 ? document.close_brace
                                 : ']';
    const char sustain_indicator = sanitize_sustain_indicator(document.sustain_indicator);
    const std::string id = sanitize_song_id(
        document.id.empty() ? path.stem().string() : std::string(document.id)
    );
    const std::string display_name = normalize_display_name_value(
        document.display_name.empty() ? path.stem().string() : std::string(document.display_name)
    );

    out << kModernMarker << '\n';
    out << "id=" << id << '\n';
    out << "name=" << display_name << '\n';
    out << "grouping=" << grouping_token(open_brace, close_brace) << '\n';
    out << "sustain=" << sustain_indicator << '\n';
    out << kMetadataSeparator << '\n';
    out << document.body;

    if (!document.body.empty() && document.body.back() != '\n') {
        out << '\n';
    }
}

} // namespace

SongRepository::SongRepository(std::filesystem::path sheet_folder) : sheet_folder_(std::move(sheet_folder)) {}

void SongRepository::ensure_storage() const {
    std::error_code error;
    std::filesystem::create_directories(sheet_folder_, error);
    migrate_legacy_files_if_needed();
}

std::string SongRepository::to_lower(const std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered;
}

std::string SongRepository::normalize_display_name(const std::string_view name) {
    return normalize_display_name_value(name);
}

std::filesystem::path SongRepository::make_unique_path(const std::string_view base_id) const {
    const std::string safe_id = sanitize_song_id(base_id);
    std::filesystem::path candidate = sheet_folder_ / (safe_id + std::string(kSongDataExtension));

    int suffix = 2;
    while (std::filesystem::exists(candidate)) {
        candidate = sheet_folder_ / (safe_id + "-" + std::to_string(suffix) + std::string(kSongDataExtension));
        ++suffix;
    }

    return candidate;
}

std::vector<Song> SongRepository::list_songs(const std::string_view filter) const {
    std::vector<Song> songs;

    migrate_legacy_files_if_needed();

    if (!std::filesystem::exists(sheet_folder_)) {
        return songs;
    }

    const std::string lowered_filter = to_lower(trim(filter));
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(sheet_folder_)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::string extension = to_lower(entry.path().extension().string());
        if (extension != kSongDataExtensionLower && extension != kLegacySongDataExtensionLower) {
            continue;
        }

        const SongDocument document = read_song_document(entry.path());
        const std::string display_name = normalize_display_name(
            document.display_name.empty() ? entry.path().stem().string() : document.display_name
        );
        if (!lowered_filter.empty() && to_lower(display_name).find(lowered_filter) == std::string::npos) {
            continue;
        }

        Song song{};
        song.id = sanitize_song_id(document.id.empty() ? entry.path().stem().string() : document.id);
        song.name = display_name;
        song.file_name = entry.path().filename().string();
        song.open_brace = document.open_brace;
        song.close_brace = document.close_brace;
        song.sustain_indicator = document.sustain_indicator;

        songs.push_back(std::move(song));
    }

    std::sort(songs.begin(), songs.end(), [](const Song& lhs, const Song& rhs) {
        const std::string left = SongRepository::to_lower(lhs.name);
        const std::string right = SongRepository::to_lower(rhs.name);
        if (left == right) {
            return lhs.id < rhs.id;
        }
        return left < right;
    });

    return songs;
}

std::vector<NoteGroup> SongRepository::load_sheet(const Song& song) const {
    migrate_legacy_files_if_needed();

    const std::filesystem::path path = sheet_folder_ / song.file_name;
    const SongDocument document = read_song_document(path);
    return parse_sheet(
        document.body + " ",
        document.open_brace,
        document.close_brace,
        document.sustain_indicator
    );
}

std::string SongRepository::load_raw_sheet_text(const Song& song) const {
    migrate_legacy_files_if_needed();

    const std::filesystem::path path = sheet_folder_ / song.file_name;
    return read_song_document(path).body;
}

std::string SongRepository::import_song(
    const std::string_view requested_name,
    const std::string_view raw_sheet_data,
    const char open_brace,
    const char close_brace,
    const char sustain_indicator
) const {
    ensure_storage();

    const std::string display_name = normalize_display_name(requested_name);
    const char normalized_sustain = sanitize_sustain_indicator(sustain_indicator);
    const std::string base_id = build_song_id(
        display_name,
        raw_sheet_data,
        open_brace,
        close_brace,
        normalized_sustain
    );
    const std::filesystem::path target_path = make_unique_path(base_id);

    SongDocument document{};
    document.id = target_path.stem().string();
    document.display_name = display_name;
    document.open_brace = open_brace;
    document.close_brace = close_brace;
    document.sustain_indicator = normalized_sustain;
    document.body = std::string(raw_sheet_data);
    write_song_document(target_path, document);

    return document.id;
}

std::string SongRepository::rename_song(const Song& song, const std::string_view new_name) const {
    const std::filesystem::path path = sheet_folder_ / song.file_name;
    SongDocument document = read_song_document(path);
    document.id = sanitize_song_id(song.id.empty() ? path.stem().string() : song.id);
    document.display_name = normalize_display_name(new_name);
    document.open_brace = song.open_brace;
    document.close_brace = song.close_brace;
    document.sustain_indicator = song.sustain_indicator;
    write_song_document(path, document);
    return document.display_name;
}

void SongRepository::delete_song(const Song& song) const {
    const std::filesystem::path path = sheet_folder_ / song.file_name;
    std::error_code error;
    std::filesystem::remove(path, error);
}

void SongRepository::update_song_contents(const Song& song, const std::string_view raw_sheet_data) const {
    const std::filesystem::path path = sheet_folder_ / song.file_name;
    SongDocument document = read_song_document(path);
    document.id = sanitize_song_id(song.id.empty() ? path.stem().string() : song.id);
    document.display_name = normalize_display_name(song.name);
    document.open_brace = song.open_brace;
    document.close_brace = song.close_brace;
    document.sustain_indicator = song.sustain_indicator;
    document.body = std::string(raw_sheet_data);
    write_song_document(path, document);
}

void SongRepository::migrate_legacy_files_if_needed() const {
    if (migration_checked_) {
        return;
    }
    migration_checked_ = true;

    if (!std::filesystem::exists(sheet_folder_)) {
        return;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(sheet_folder_)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string extension = to_lower(entry.path().extension().string());
        if (extension != kLegacySongDataExtensionLower && extension != kSongDataExtensionLower) {
            continue;
        }

        try {
            std::filesystem::path working_path = entry.path();
            if (extension == kLegacySongDataExtensionLower) {
                std::filesystem::path target_path =
                    working_path.parent_path() / (working_path.stem().string() + std::string(kSongDataExtension));
                int suffix = 2;
                while (std::filesystem::exists(target_path)) {
                    target_path = working_path.parent_path() /
                                  (working_path.stem().string() + " (" + std::to_string(suffix) + ")" +
                                   std::string(kSongDataExtension));
                    ++suffix;
                }

                std::error_code rename_error;
                std::filesystem::rename(working_path, target_path, rename_error);
                if (rename_error) {
                    continue;
                }
                working_path = target_path;
            }

            SongDocument document = read_song_document(working_path);
            const bool missing_id = document.id.empty();
            const bool missing_name = document.display_name.empty();
            if (!document.is_modern || missing_id || missing_name) {
                if (missing_id) {
                    document.id = sanitize_song_id(working_path.stem().string());
                }
                if (missing_name) {
                    document.display_name = normalize_display_name(working_path.stem().string());
                }
                write_song_document(working_path, document);
            }
        } catch (const std::exception&) {
        }
    }
}

} // namespace piano_assist
