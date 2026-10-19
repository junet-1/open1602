// SPDX-License-Identifier: GPL-3.0-or-later
//
// High score lists and campaign progress (docs/re/missions.md "Kampagnen").
//
// The original writes both into its installation: the list of a scenario to Szenes/<name>.hss
// (HighScore_Insert 0x0048E540, twelve lines), the progress into Game.dat. Open1602 never writes
// there (T-005): it keeps its own copies in the user's data folder, in the same text format for the
// lists, and starts a list from the original's file the first time it adds a line.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "o1602/assets/scenario.hpp"

namespace o1602::assets {

/// Lines of a list in the original's files.
inline constexpr std::size_t kHighScoreLines = 12;

/// The text of a .hss file: "Objekt: HISCORE", one "Entry: i, \"name\", points, stars, game id"
/// per line, padded with empty lines to kHighScoreLines, "EndObj;".
[[nodiscard]] std::string write_high_scores(const std::vector<HighScore>& scores);

/// Puts `entry` into the list, best first, after lines with the same points, and keeps
/// kHighScoreLines. A line of the same game and name (the same game ended again after loading a
/// savegame) is replaced when the new points are higher and stays otherwise, so one game holds
/// one line [Open1602; how the original treats it is open]. The index of the entry's line, or
/// nothing if it did not make the list.
std::optional<std::size_t> insert_high_score(std::vector<HighScore>& scores, const HighScore& entry);

/// Which missions a player has opened: per campaign the next part, and the next numbered
/// mission of the original scenarios (Game.dat lines "Kampagne: k, n" and the third value of
/// "Volume:" minus 597 in the original).
struct CampaignProgress {
    static constexpr std::uint32_t kFirstNumbered = 2;  ///< what counts without a value
    static constexpr std::uint32_t kLastNumbered = 17;
    std::map<std::uint32_t, std::uint32_t> campaigns;  ///< campaign -> next part
    std::uint32_t numbered = kFirstNumbered;

    /// A single player game won (GameDat_Write 0x00487FB0): a campaign opens `part + 1` if that
    /// is further than before; a numbered mission n opens n + 1 when it was the furthest
    /// (at most kLastNumbered). True if something changed.
    bool record_win(std::uint32_t campaign, std::uint32_t part, std::uint32_t mission_number);
};
/// Lines "campaign k n" and "numbered n"; unknown lines are passed over.
[[nodiscard]] CampaignProgress parse_progress(std::string_view text);
[[nodiscard]] std::string write_progress(const CampaignProgress& progress);

}  // namespace o1602::assets
