// SPDX-License-Identifier: GPL-3.0-or-later
//
// Players of a game. Unlike the original (7 fixed slots, colours 0..6, owner 7 = nobody) the number
// of players and banners is open (ADR 0007); the original values are translated when loading.
#pragma once

#include <cstdint>
#include <string>

#include "o1602/assets/scenario.hpp"

namespace o1602::sim {

/// Index into World::players(). 0xFF means "nobody", so a game can have up to 254 players.
using PlayerId = std::uint8_t;
inline constexpr PlayerId kNoPlayer = 0xFF;

/// Banner of a player: display colour and flag sprites (render::BannerTable). The base game
/// provides the original colours 0..6 as banners 0..6; mods add more.
using BannerId = std::uint16_t;

/// Role of a player slot. Stands for the controller (human, computer) and the fixed factions of
/// the original until factions become data (ADR 0007, section 3).
enum class PlayerRole : std::uint8_t {
    Human,
    Computer,
    Trader,  ///< free trader
    Pirates,
    Natives,
};

struct Player {
    PlayerRole role = PlayerRole::Human;
    BannerId banner = 0;
    std::int32_t money = 0;
    std::string name;
    /// Unlocked build levels: bit n - 1 = INFRA number n (BuildingTable::infra_levels()). A level
    /// stays unlocked once one city of the player reached it (0x0047F8A0).
    std::uint32_t infra_mask = 0;
    /// Buildings the player may build only once or by allowance (record Bauinfra SCHLOSS,
    /// KATHETRALE, TRIUMPH, DENKMAL).
    std::uint8_t castles_built = 0;
    std::uint8_t cathedrals_built = 0;
    std::uint16_t arches_allowed = 0;
    std::uint16_t arches_built = 0;
    std::uint16_t monuments_allowed = 0;
    std::uint16_t monuments_built = 0;

    [[nodiscard]] bool unlocked(std::uint32_t infra) const noexcept {
        return infra == 0 || (infra <= 32 && (infra_mask >> (infra - 1) & 1U) != 0);
    }
};

/// Owner number of the original files (island tile attribute, SHIP4, PLAYER4 slot): 0..6 are the
/// slots of PLAYER4, everything else (7 = nobody) has no owner.
[[nodiscard]] constexpr PlayerId player_from_original(std::uint8_t owner) noexcept {
    return owner < assets::kOriginalPlayerSlots ? owner : kNoPlayer;
}

[[nodiscard]] constexpr PlayerRole role_from_original(assets::PlayerKind kind) noexcept {
    switch (kind) {
        case assets::PlayerKind::Computer:
            return PlayerRole::Computer;
        case assets::PlayerKind::Trader:
            return PlayerRole::Trader;
        case assets::PlayerKind::Pirate:
            return PlayerRole::Pirates;
        case assets::PlayerKind::Natives:
            return PlayerRole::Natives;
        case assets::PlayerKind::Human:
            break;
    }
    return PlayerRole::Human;
}

/// Original colours map 1:1 to the banners of the base game.
[[nodiscard]] constexpr BannerId banner_from_original(assets::PlayerColor color) noexcept {
    return static_cast<BannerId>(color);
}

[[nodiscard]] const char* role_name(PlayerRole role) noexcept;

}  // namespace o1602::sim
