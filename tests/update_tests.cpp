// SPDX-License-Identifier: MIT

#include <doctest/doctest.h>

#include <vector>

#include "piano_assist/update_utils.hpp"

TEST_CASE("version comparison accepts v-prefix and numeric segment padding") {
    using piano_assist::is_remote_version_newer;

    CHECK(is_remote_version_newer("v1.6.0", "1.5.9"));
    CHECK(is_remote_version_newer("1.5.2.1", "1.5.2"));
    CHECK_FALSE(is_remote_version_newer("1.5.2", "1.5.2"));
    CHECK_FALSE(is_remote_version_newer("1.5.1", "1.5.2"));
    CHECK_FALSE(is_remote_version_newer("latest", "1.5.2"));
}

TEST_CASE("best release asset prefers setup exe over other artifacts") {
    using piano_assist::pick_best_release_asset;
    using piano_assist::ReleaseAssetInfo;

    const std::vector<ReleaseAssetInfo> assets{
        {"SheetMaster-1.6.0-windows-portable.zip", "https://example.com/portable.zip", ""},
        {"SheetMaster-1.6.0-setup.exe", "https://example.com/setup.exe", "sha256:ABCDEF123456"},
        {"SheetMaster-1.6.0.msi", "https://example.com/setup.msi", ""}};

    const auto best = pick_best_release_asset(assets);
    CHECK(best.has_download);
    CHECK(best.is_installer);
    CHECK(best.name == "SheetMaster-1.6.0-setup.exe");
    CHECK(best.download_url == "https://example.com/setup.exe");
    CHECK(best.sha256_hex == "abcdef123456");
}

TEST_CASE("asset picker falls back to msi then zip") {
    using piano_assist::pick_best_release_asset;
    using piano_assist::ReleaseAssetInfo;

    const auto msi_best = pick_best_release_asset(
        {ReleaseAssetInfo{"SheetMaster-1.6.0.msi", "https://example.com/setup.msi", ""}});
    CHECK(msi_best.has_download);
    CHECK(msi_best.is_installer);
    CHECK(msi_best.name == "SheetMaster-1.6.0.msi");

    const auto zip_best = pick_best_release_asset({ReleaseAssetInfo{
        "SheetMaster-1.6.0-windows-portable.zip", "https://example.com/portable.zip", ""}});
    CHECK(zip_best.has_download);
    CHECK_FALSE(zip_best.is_installer);
    CHECK(zip_best.name == "SheetMaster-1.6.0-windows-portable.zip");
}
