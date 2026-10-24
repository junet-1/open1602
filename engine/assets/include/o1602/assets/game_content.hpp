// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the game, its tools and a server share to start the same game (LC-17): the fingerprint of
// the data the simulation computes with, and the packages of the mods folder in one order.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "o1602/assets/game_directory.hpp"

namespace o1602::assets {

/// The data the simulation computes with, as one number the computers of a multiplayer game
/// compare before they play: the bytes of haeuser.cod and figuren.cod. The names of new cities and
/// ships travel with the game (sim::Simulation::offer_names), so installations in other languages
/// play together; the corrections of assets::fixes change nothing the simulation reads and may
/// differ (data_fixes.hpp).
[[nodiscard]] std::uint64_t content_fingerprint(const GameDirectory& game);

/// A folder of <data folder>/mods: its id is the folder's name.
struct ModPackage {
    std::string id;  ///< UTF-8
    std::filesystem::path folder;
};

/// The packages in `mods`, ordered by the bytes of their UTF-8 names, the same on every system
/// (std::filesystem::path compares wchar_t on Windows); none if the folder does not exist.
[[nodiscard]] std::vector<ModPackage> mod_packages(const std::filesystem::path& mods);

}  // namespace o1602::assets
