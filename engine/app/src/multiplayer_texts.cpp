// SPDX-License-Identifier: GPL-3.0-or-later
#include "multiplayer_texts.hpp"

#include <array>
#include <utility>

#include "app_settings.hpp"

namespace o1602::app {

namespace {

struct Entry {
    MpText id;
    const char* english;
    const char* german;
};

// In the order of MpText.
constexpr std::array kTexts = {
    Entry{MpText::Multiplayer, "Multiplayer", "Mehrspieler"},
    Entry{MpText::YourName, "Your name", "Euer Name"},
    Entry{MpText::Games, "Games", "Spiele"},
    Entry{MpText::ColGame, "Game", "Spiel"},
    Entry{MpText::ColHost, "Host", "Gastgeber"},
    Entry{MpText::ColPlayers, "Players", "Spieler"},
    Entry{MpText::ColState, "State", "Stand"},
    Entry{MpText::ColPing, "Ping", "Ping"},
    Entry{MpText::StateLobby, "Lobby", "Lobby"},
    Entry{MpText::StateRunning, "running", "läuft"},
    Entry{MpText::StateRunningJoin, "running, join", "läuft, Einstieg"},
    Entry{MpText::StateRunningWatch, "running, watch", "läuft, Zuschauen"},
    Entry{MpText::StateOtherVersion, "other version", "andere Version"},
    Entry{MpText::NoAnswer, "no answer", "keine Antwort"},
    Entry{MpText::NoGamesFound, "No games found yet.", "Noch keine Spiele gefunden."},
    Entry{MpText::SearchHint, "Games on the local network appear by themselves.",
          "Spiele im lokalen Netz erscheinen von selbst."},
    Entry{MpText::ConnectDirectly, "Connect directly", "Direkt verbinden"},
    Entry{MpText::HostAddress, "Address of the host", "Adresse des Gastgebers"},
    Entry{MpText::Password, "Password", "Kennwort"},
    Entry{MpText::Connect, "Connect", "Verbinden"},
    Entry{MpText::CreateGame, "Create game", "Spiel erstellen"},
    Entry{MpText::HostHint, "Players on the internet need UDP port {} open at the host.",
          "Mitspieler im Internet brauchen UDP-Port {} offen beim Gastgeber."},
    Entry{MpText::Back, "Back", "Zurück"},
    Entry{MpText::Refresh, "Refresh", "Aktualisieren"},
    Entry{MpText::Join, "Join", "Beitreten"},
    Entry{MpText::CreateTitle, "Create game", "Spiel erstellen"},
    Entry{MpText::NewEndless, "New endless game", "Neues Endlosspiel"},
    Entry{MpText::Scenario, "Scenario", "Szenario"},
    Entry{MpText::Savegame, "Savegame", "Spielstand"},
    Entry{MpText::Level, "Level", "Stufe"},
    Entry{MpText::WorldNumber, "World no.", "Welt Nr."},
    Entry{MpText::Roll, "Roll", "Würfeln"},
    Entry{MpText::EndlessHint, "The same number makes the same world.", "Gleiche Nummer, gleiche Inselwelt."},
    Entry{MpText::ScenarioHint, "The others do not need the file: the game comes from the host.",
          "Die Mitspieler brauchen die Datei nicht: Das Spiel kommt vom Gastgeber."},
    Entry{MpText::SavegameHint, "Whoever comes back takes a free slot of the game.",
          "Wer zurückkommt, nimmt einen freien Platz des Spiels."},
    Entry{MpText::NoSavegames, "No savegames.", "Keine Spielstände."},
    Entry{MpText::Session, "Session", "Sitzung"},
    Entry{MpText::GameTitle, "Name of the game", "Name des Spiels"},
    Entry{MpText::PasswordOptional, "Password (empty: anyone)", "Kennwort (leer: jeder)"},
    Entry{MpText::ShowOnLan, "Show on the local network", "Im lokalen Netz zeigen"},
    Entry{MpText::AllowJoinRunning, "Joining the running game", "Einstieg ins laufende Spiel"},
    Entry{MpText::AllowSpectators, "Spectators", "Zuschauer"},
    Entry{MpText::HostOnly, "Host without a player", "Nur Gastgeber, ohne Platz"},
    Entry{MpText::Port, "UDP port", "UDP-Port"},
    Entry{MpText::OpenLobby, "Open lobby", "Lobby öffnen"},
    Entry{MpText::DefaultTitle, "{}'s game", "Spiel von {}"},
    Entry{MpText::EndlessGame, "Endless game", "Endlosspiel"},
    Entry{MpText::LevelN, "level {}", "Stufe {}"},
    Entry{MpText::Watchers, "Watching: {}", "Zuschauer: {}"},
    Entry{MpText::Nobody, "nobody", "niemand"},
    Entry{MpText::SeatOpen, "Open", "Offen"},
    Entry{MpText::SeatComputer, "Computer", "Computer"},
    Entry{MpText::SeatClosed, "Closed", "Geschlossen"},
    Entry{MpText::Ready, "Ready", "Bereit"},
    Entry{MpText::Choosing, "choosing", "wählt noch"},
    Entry{MpText::Free, "free", "frei"},
    Entry{MpText::Take, "Take", "Nehmen"},
    Entry{MpText::Kick, "Remove", "Entfernen"},
    Entry{MpText::You, "(you)", "(Ihr)"},
    Entry{MpText::Host, "(host)", "(Gastgeber)"},
    Entry{MpText::Chat, "Chat", "Chat"},
    Entry{MpText::ChatHint, "Message to all (Enter sends)", "Nachricht an alle (Eingabe sendet)"},
    Entry{MpText::Leave, "Leave", "Verlassen"},
    Entry{MpText::Start, "Start game", "Spiel starten"},
    Entry{MpText::StartHint, "Start once every player is ready.", "Starten, sobald alle Spieler bereit sind."},
    Entry{MpText::WaitHostStart, "The host starts once everyone is ready.",
          "Der Gastgeber startet, sobald alle bereit sind."},
    Entry{MpText::Watch, "Watch", "Zuschauen"},
    Entry{MpText::Yes, "yes", "ja"},
    Entry{MpText::No, "no", "nein"},
    Entry{MpText::Connecting, "Connecting to {} ...", "Verbinde mit {} ..."},
    Entry{MpText::PlayerJoined, "{} joined.", "{} ist beigetreten."},
    Entry{MpText::PlayerLeft, "{} left.", "{} ist gegangen."},
    Entry{MpText::Lagging, "{} lags behind; the game goes on.", "{} hängt; das Spiel geht weiter."},
    Entry{MpText::Desync, "{} diverged and gets the game anew.", "{} wich ab und bekommt das Spiel neu."},
    Entry{MpText::Waiting, "Waiting for the other players ...", "Warte auf die Mitspieler ..."},
    Entry{MpText::HostPort, "Hosting on UDP port {}", "Gastgeber auf UDP-Port {}"},
    Entry{MpText::CannotHost, "Cannot host: {}", "Kein Spiel möglich: {}"},
    Entry{MpText::CannotConnect, "Cannot connect: {}", "Keine Verbindung: {}"},
    Entry{MpText::StartFailed, "The game cannot start: {}", "Das Spiel startet nicht: {}"},
};

constexpr bool in_order() {
    for (std::size_t i = 0; i < kTexts.size(); ++i) {
        if (static_cast<std::size_t>(kTexts[i].id) != i) {
            return false;
        }
    }
    return true;
}
static_assert(in_order(), "kTexts must list every MpText in order");
static_assert(kTexts.size() == static_cast<std::size_t>(MpText::StartFailed) + 1, "a text is missing");

std::string pick(const char* english, const char* german, Language language) {
    return utf8_to_cp1252(language == Language::German ? german : english);
}

}  // namespace

Language language_of(const assets::TextTable* texts) {
    if (texts == nullptr) {
        return Language::English;
    }
    // The German edition's menu texts speak of "Spiel", the English one's of "game".
    int german = 0;
    int english = 0;
    for (const std::string& line : texts->section("GAME")) {
        german += line.find("Spiel") != std::string::npos ? 1 : 0;
        english += line.find("ame") != std::string::npos && line.find("Spiel") == std::string::npos ? 1 : 0;
    }
    return german >= english && german > 0 ? Language::German : Language::English;
}

std::string mp_text(MpText id, Language language) {
    const Entry& e = kTexts[static_cast<std::size_t>(id)];
    return pick(e.english, e.german, language);
}

std::string mp_text(MpText id, Language language, std::string_view value) {
    std::string s = mp_text(id, language);
    if (const std::size_t at = s.find("{}"); at != std::string::npos) {
        s.replace(at, 2, value);
    }
    return s;
}

std::string mp_text(net::EndReason reason, Language language) {
    switch (reason) {
        case net::EndReason::None:
            return {};
        case net::EndReason::ConnectFailed:
            return pick("The host cannot be reached.", "Der Gastgeber ist nicht erreichbar.", language);
        case net::EndReason::HostLeft:
            return pick("The connection to the host ended.", "Die Verbindung zum Gastgeber ist beendet.", language);
        case net::EndReason::Refused:
            return pick("The host refused.", "Der Gastgeber hat abgelehnt.", language);
        case net::EndReason::ProtocolError:
            return pick("A message from the network made no sense.", "Unverständliche Nachricht im Netz.", language);
        case net::EndReason::LoadFailed:
            return pick("The host's game could not be loaded.", "Das Spiel des Gastgebers ließ sich nicht laden.",
                        language);
        case net::EndReason::Desync:
            return pick("This computer keeps diverging from the host.",
                        "Dieser Rechner weicht immer wieder vom Gastgeber ab.", language);
    }
    return {};
}

std::string mp_text(net::RefuseReason reason, Language language) {
    switch (reason) {
        case net::RefuseReason::Version:
            return pick("The host has another version of Open1602.",
                        "Der Gastgeber hat eine andere Version von Open1602.", language);
        case net::RefuseReason::Content:
            return pick("The host has other game data.", "Der Gastgeber hat andere Spieldaten.", language);
        case net::RefuseReason::Full:
            return pick("No free slot.", "Kein Platz mehr frei.", language);
        case net::RefuseReason::Closed:
            return pick("The host takes nobody into the running game.",
                        "Der Gastgeber nimmt niemanden mehr ins laufende Spiel.", language);
        case net::RefuseReason::Desync:
            return pick("This computer keeps diverging from the host.",
                        "Dieser Rechner weicht immer wieder vom Gastgeber ab.", language);
        case net::RefuseReason::Invalid:
            return pick("This computer sent what makes no sense.", "Dieser Rechner hat Unverständliches geschickt.",
                        language);
        case net::RefuseReason::Password:
            return pick("Wrong password.", "Falsches Kennwort.", language);
        case net::RefuseReason::Kicked:
            return pick("The host sent you away.", "Der Gastgeber hat Euch entfernt.", language);
    }
    return {};
}

}  // namespace o1602::app
