// SPDX-License-Identifier: GPL-3.0-or-later
//
// The main menu and the screens before a game (docs/re/ui.md "Main menu"; Menu_OnClick 0x0048B590,
// Menu_Redraw 0x0048ABD0): BASE (main menu), HOST (list of scenarios or savegames), MISSION
// (briefing, choice of level or part, high scores) and FARBWAHL (name and banner), and for the
// endless game Open1602's own start dialog with the game's settings (ADR 0008). The button
// "Mehrspieler" opens Open1602's own multiplayer menu instead of the original's screens. The menu shows the screens and
// answers with MenuEvents; loading and starting is the viewer's.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app_settings.hpp"
#include "endless_setup.hpp"
#include "game_settings_menu.hpp"

#include "o1602/assets/gad.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/high_scores.hpp"
#include "o1602/assets/scenario.hpp"
#include "o1602/assets/text_table.hpp"
#include "o1602/ui/panel.hpp"

namespace o1602::app {

/// One entry of the scenario list (0x0048E710): the files of one name without trailing digits.
struct MenuScenario {
    std::string name;            ///< base name in Windows-1252 (the fonts')
    std::filesystem::path base;  ///< the same as a file name without folder (for the .hss beside it)
    /// The files by their first digit (level of the endless game, part of a campaign); one file
    /// without digits has the key 0xFF.
    std::vector<std::pair<std::uint8_t, std::filesystem::path>> files;
    assets::ScenarioHeader header;
    bool separator = false;  ///< a heading line ("----Kampagnen----")
};

/// The scenarios of the game folder in the menu's order: originals (tutorial, endless game,
/// numbered missions by number), campaigns, the other adventures by stars, each group under its
/// heading (`headings`: [GAME] 85..88).
[[nodiscard]] std::vector<MenuScenario> scenario_list(const std::filesystem::path& folder,
                                                      const std::array<std::string, 4>& headings);

struct MenuEvent {
    enum class Kind : std::uint8_t {
        Quit,          ///< leave the program (after a question)
        Credits,       ///< video 10 in the picture frame
        Intro,         ///< video 58
        NewEndless,    ///< endless game of `level` with `colour`, `name` and the settings `endless`
        Scenario,      ///< start `path` with `name`
        LoadSlot,      ///< savegame `slot`
        ContinueLast,  ///< lastgame
        NameEntered,   ///< the name is confirmed (Enter): "Wählt Euer Banner" and the like
        Multiplayer    ///< the multiplayer menu (Open1602's own, multiplayer_menu.hpp)
    };
    Kind kind = Kind::Quit;
    std::uint8_t level = 0;
    std::uint8_t colour = 0;
    std::string name{};
    std::filesystem::path path{};
    std::size_t slot = 0;
    EndlessSetup endless{};
    /// Scenario: the chapter of the tutorial, nothing for another scenario.
    std::optional<std::uint8_t> tutorial_chapter{};
};

class MainMenu {
public:
    enum class Screen : std::uint8_t { Base, Host, Mission, Colour, Settings };

    MainMenu(const assets::GameDirectory& game, const assets::TextTable* texts);

    [[nodiscard]] bool usable() const noexcept { return m_base.has_value() && m_host.has_value(); }
    [[nodiscard]] Screen screen() const noexcept { return m_screen; }
    void open();  ///< the main menu again
    void set_scale(double scale);
    /// Savegame names for the list "Spiel laden" and the player's name for the next game.
    void set_slots(const SaveSlots& slots) { m_slots = slots; }
    void set_name(std::string name) { m_name = std::move(name); }
    /// Open1602's own high score lists (<folder>/<scenario>.hss); the mission screen shows them
    /// instead of the original's once a game has added a line.
    void set_score_folder(std::filesystem::path folder) { m_score_folder = std::move(folder); }
    /// Open1602's own record of opened missions (progress.cfg; the original keeps it in Game.dat):
    /// the list leaves out numbered missions above it, the mission screen disables campaign parts.
    void set_progress_file(std::filesystem::path file) { m_progress_file = std::move(file); }
    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    /// The settings the start dialog of the endless game offers first (the last ones chosen).
    void set_endless_setup(const EndlessSetup& setup) { m_endless = setup; }
    [[nodiscard]] const EndlessSetup& endless_setup() const noexcept { return m_endless; }
    /// Colours of Open1602's own screens (the start dialog).
    void set_form_style(const ui::FormStyle& style) { m_settings.set_style(style); }
    /// The chapter of the tutorial the mission screen offers first (the settings' "Tutornr").
    void set_tutorial_chapter(std::uint8_t chapter) noexcept { m_tutorial_chapter = chapter; }
    /// The file of a chapter of the tutorial, empty if there is none.
    [[nodiscard]] std::filesystem::path tutorial_file(std::uint8_t chapter);
    /// "Neues Spiel" on the end screen: the mission screen of the scenario called `name`
    /// (Windows-1252), else the main menu. True if the scenario was found.
    bool open_mission(std::string_view name);

    /// The screen's gadgets, relative to the full screen region.
    [[nodiscard]] std::vector<ui::DrawItem> draw(const std::array<const ui::Font*, 3>& fonts);
    std::vector<MenuEvent> mouse(std::int32_t x, std::int32_t y, bool down);
    /// Escape (0x00409D70): one screen back; false in the main menu.
    bool back();
    /// Where the credits play: the picture frame of the main menu, design units.
    [[nodiscard]] static ui::Rect video_frame() noexcept { return {500, 359, 453, 367}; }

    [[nodiscard]] bool typing() const noexcept { return m_typing; }
    void type(std::string_view text);
    /// Backspace 8, Enter 13 (confirms: NameEntered), Escape 27 (back to the choice of the game).
    std::optional<MenuEvent> key(int key);
    /// The name the field shows when none was typed yet ("Namenlos" in German).
    [[nodiscard]] static const std::string& default_name();

private:
    enum class ListMode : std::uint8_t { New, Load };
    [[nodiscard]] ui::Panel* panel();
    [[nodiscard]] std::string text(std::string_view section, std::size_t line) const;
    void refresh_host();
    void refresh_mission();
    void refresh_colour();
    void load_scenarios();
    /// Parts of a campaign that may be played (all for other scenarios).
    [[nodiscard]] std::size_t campaign_parts_open(const MenuScenario& s) const;
    void select_scenario(std::size_t index);
    /// Briefing and high scores of the chosen scenario and part.
    void load_details();
    [[nodiscard]] std::filesystem::path chosen_file() const;

    const assets::GameDirectory* m_game;
    const assets::TextTable* m_texts;
    std::optional<ui::Panel> m_base;
    std::optional<ui::Panel> m_host;
    std::optional<ui::Panel> m_mission;
    std::optional<ui::Panel> m_colour;
    Screen m_screen = Screen::Base;
    ListMode m_mode = ListMode::New;
    std::vector<MenuScenario> m_scenarios;
    std::size_t m_first = 0;                  ///< first row of the list
    std::size_t m_chosen = 0;                 ///< scenario of the mission screen
    std::uint8_t m_variant = 0xFF;            ///< its first digit
    std::string m_briefing;                   ///< AUFTRAG4 text of the human's mission
    std::vector<assets::HighScore> m_scores;  ///< of the chosen scenario
    std::size_t m_scores_first = 0;
    std::int32_t m_briefing_line = 0;
    SaveSlots m_slots;
    std::filesystem::path m_score_folder;
    std::filesystem::path m_progress_file;
    assets::CampaignProgress m_progress;
    std::string m_name;
    GameSettingsMenu m_settings;
    EndlessSetup m_endless;
    std::uint8_t m_banner = 0;  ///< chosen on the colour screen, for the start dialog
    std::uint8_t m_tutorial_chapter = 0;
    bool m_name_fresh = false;  ///< the name was put in, not typed: the first key replaces it
    bool m_typing = false;
};

}  // namespace o1602::app
