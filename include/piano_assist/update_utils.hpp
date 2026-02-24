// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace piano_assist {

struct ReleaseAssetInfo {
    std::string name;
    std::string download_url;
    std::string digest;
};

struct SelectedReleaseAsset {
    std::string name;
    std::string download_url;
    std::string sha256_hex;
    bool is_installer{false};
    bool has_download{false};
};

[[nodiscard]] bool is_remote_version_newer(std::string_view remote_tag,
                                           std::string_view local_version);

[[nodiscard]] int update_asset_priority(std::string_view asset_name);

[[nodiscard]] bool is_installer_asset_name(std::string_view asset_name);

[[nodiscard]] SelectedReleaseAsset pick_best_release_asset(
    const std::vector<ReleaseAssetInfo>& assets);

} // namespace piano_assist

