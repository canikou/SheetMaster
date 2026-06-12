// SPDX-License-Identifier: MIT

#include "piano_assist/update_utils.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <vector>

namespace piano_assist {
namespace {

std::string trim_copy(std::string_view text) {
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }

    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return std::string(text.substr(start, end - start));
}

std::string to_lower_copy(std::string_view text) {
    std::string lowered(text);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return lowered;
}

void maybe_push_segment(std::vector<int>* segments, std::string* current_digits) {
    if (segments == nullptr || current_digits == nullptr || current_digits->empty()) {
        return;
    }

    int value = 0;
    for (const char ch : *current_digits) {
        const int digit = ch - '0';
        if (value > (std::numeric_limits<int>::max() - digit) / 10) {
            value = std::numeric_limits<int>::max();
            break;
        }
        value = (value * 10) + digit;
    }
    segments->push_back(value);
    current_digits->clear();
}

std::vector<int> parse_version_segments(std::string_view version) {
    std::string cleaned = trim_copy(version);
    if (!cleaned.empty() && (cleaned.front() == 'v' || cleaned.front() == 'V')) {
        cleaned.erase(cleaned.begin());
    }

    std::vector<int> segments;
    std::string current_digits;
    bool started = false;

    for (const char ch : cleaned) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            current_digits.push_back(ch);
            started = true;
            continue;
        }
        if (ch == '.') {
            if (!started) {
                continue;
            }
            maybe_push_segment(&segments, &current_digits);
            continue;
        }
        if (started) {
            break;
        }
    }

    maybe_push_segment(&segments, &current_digits);
    return segments;
}

bool ends_with(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.substr(value.size() - suffix.size(), suffix.size()) == suffix;
}

} // namespace

bool is_remote_version_newer(const std::string_view remote_tag,
                             const std::string_view local_version) {
    const std::vector<int> remote_segments = parse_version_segments(remote_tag);
    const std::vector<int> local_segments = parse_version_segments(local_version);
    if (remote_segments.empty() || local_segments.empty()) {
        return false;
    }

    const std::size_t max_size = std::max(remote_segments.size(), local_segments.size());
    for (std::size_t index = 0; index < max_size; ++index) {
        const int remote = (index < remote_segments.size()) ? remote_segments[index] : 0;
        const int local = (index < local_segments.size()) ? local_segments[index] : 0;
        if (remote != local) {
            return remote > local;
        }
    }

    return false;
}

int update_asset_priority(const std::string_view asset_name) {
    if (asset_name.empty()) {
        return std::numeric_limits<int>::max();
    }

    const std::string lowered = to_lower_copy(asset_name);
    const bool installer_named = lowered.find("setup") != std::string::npos ||
                                 lowered.find("installer") != std::string::npos;

    if (ends_with(lowered, ".exe") && installer_named) {
        return 0;
    }
    if (ends_with(lowered, ".msi")) {
        return 1;
    }
    if (ends_with(lowered, ".zip")) {
        return 2;
    }
    if (ends_with(lowered, ".exe")) {
        return 3;
    }

    return std::numeric_limits<int>::max();
}

bool is_installer_asset_name(const std::string_view asset_name) {
    const std::string lowered = to_lower_copy(asset_name);
    if (ends_with(lowered, ".msi")) {
        return true;
    }
    return ends_with(lowered, ".exe") && (lowered.find("setup") != std::string::npos ||
                                          lowered.find("installer") != std::string::npos);
}

SelectedReleaseAsset pick_best_release_asset(const std::vector<ReleaseAssetInfo>& assets) {
    SelectedReleaseAsset best{};
    int best_priority = std::numeric_limits<int>::max();

    for (const ReleaseAssetInfo& asset : assets) {
        const std::string trimmed_name = trim_copy(asset.name);
        const std::string trimmed_url = trim_copy(asset.download_url);
        if (trimmed_name.empty() || trimmed_url.empty()) {
            continue;
        }

        const int priority = update_asset_priority(trimmed_name);
        if (priority > best_priority) {
            continue;
        }
        if (priority == best_priority && best.has_download) {
            continue;
        }

        best_priority = priority;
        best.name = trimmed_name;
        best.download_url = trimmed_url;
        best.is_installer = is_installer_asset_name(trimmed_name);
        best.has_download = true;
        best.sha256_hex.clear();

        const std::string trimmed_digest = trim_copy(asset.digest);
        const std::string lowered_digest = to_lower_copy(trimmed_digest);
        if (lowered_digest.rfind("sha256:", 0) == 0 && trimmed_digest.size() > 7) {
            best.sha256_hex = trim_copy(trimmed_digest.substr(7));
            std::transform(
                best.sha256_hex.begin(), best.sha256_hex.end(), best.sha256_hex.begin(),
                [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
        }
    }

    return best;
}

} // namespace piano_assist
