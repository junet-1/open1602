// SPDX-License-Identifier: GPL-3.0-or-later
//
// The texts of the multiplayer menu and messages, which the original's Text.cod does not have, in
// the languages of the installations: German and English. All of them are here, so that they can
// move into Open1602's localisation as one piece.
// The texts are UTF-8 here; mp_text() gives them in the fonts' Windows-1252.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "o1602/assets/text_table.hpp"
#include "o1602/net/protocol.hpp"
#include "o1602/net/session.hpp"

namespace o1602::app {

enum class Language : std::uint8_t { English, German };

/// The language of the installation, from its Text.cod (English without one).
[[nodiscard]] Language language_of(const assets::TextTable* texts);

enum class MpText : std::uint16_t {
    Multiplayer,
    YourName,
    Games,
    ColGame,
    ColHost,
    ColPlayers,
    ColState,
    ColPing,
    StateLobby,
    StateRunning,
    StateRunningJoin,
    StateRunningWatch,
    StateOtherVersion,
    NoAnswer,
    NoGamesFound,
    SearchHint,
    ConnectDirectly,
    HostAddress,
    Password,
    Connect,
    CreateGame,
    HostHint,
    Back,
    Refresh,
    Join,
    CreateTitle,
    NewEndless,
    Scenario,
    Savegame,
    Level,
    WorldNumber,
    Roll,
    EndlessHint,
    ScenarioHint,
    SavegameHint,
    NoSavegames,
    Session,
    GameTitle,
    PasswordOptional,
    ShowOnLan,
    AllowJoinRunning,
    AllowSpectators,
    HostOnly,
    Port,
    OpenLobby,
    DefaultTitle,
    EndlessGame,
    LevelN,
    Watchers,
    Nobody,
    SeatOpen,
    SeatComputer,
    SeatClosed,
    Ready,
    Choosing,
    Free,
    Take,
    Kick,
    You,
    Host,
    Chat,
    ChatHint,
    Leave,
    Start,
    StartHint,
    WaitHostStart,
    Watch,
    Yes,
    No,
    Connecting,
    PlayerJoined,
    PlayerLeft,
    Lagging,
    Desync,
    Waiting,
    HostPort,
    CannotHost,
    CannotConnect,
    StartFailed,
};

[[nodiscard]] std::string mp_text(MpText id, Language language);
/// The text with its "{}" replaced by `value` (a name, a number).
[[nodiscard]] std::string mp_text(MpText id, Language language, std::string_view value);
/// Why a session ended or a computer was refused, for the player.
[[nodiscard]] std::string mp_text(net::EndReason reason, Language language);
[[nodiscard]] std::string mp_text(net::RefuseReason reason, Language language);

}  // namespace o1602::app
