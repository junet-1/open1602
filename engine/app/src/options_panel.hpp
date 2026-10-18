// SPDX-License-Identifier: GPL-3.0-or-later
//
// The settings of the side panel (mode 30014; OptionsPanel_Refresh 0x00439630, clicks at the end of
// 0x0043CE00; docs/re/ui.md): OPTION.GAD with its pages DISK (savegames, leaving the game), MUSIK
// (music and effects) and ANNO (voice messages and videos). The panel only shows and edits an
// OptionsModel; what has to happen beyond that comes back as OptionsEvents.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "o1602/assets/gad.hpp"
#include "o1602/assets/text_table.hpp"
#include "o1602/audio/game_audio.hpp"
#include "o1602/ui/panel.hpp"

namespace o1602::app {

/// What the settings show; the viewer fills it, the panel changes `audio` and `mission_window`.
struct OptionsModel {
    audio::AudioSettings audio;
    std::vector<std::string> tracks;  ///< music titles in the order of the list
    std::string playing;              ///< title playing now
    /// The savegames by slot (empty name: free), as many as there are; the list scrolls.
    std::vector<std::string> slot_names;
    std::vector<std::uint8_t> slot_humans;  ///< human players in the savegame (icon from 2)
    std::size_t resolution = 2;             ///< 0 640 x 480, 1 800 x 600, 2 1024 x 768
    bool mission_window = false;            ///< "Auftrag ansehen"
    bool multiplayer = false;
};

/// What the settings ask of the viewer.
struct OptionsEvent {
    enum class Kind : std::uint8_t {
        Save,           ///< into slot `value`, named `text`
        Load,           ///< slot `value`
        Quit,           ///< "Ende": leave the game
        Resolution,     ///< `value` 0..2
        Audio,          ///< OptionsModel::audio changed
        PlayTrack,      ///< play `text` now (and enable it)
        TestEffects,    ///< the test sound
        MissionWindow,  ///< OptionsModel::mission_window changed
    };
    Kind kind = Kind::Audio;
    std::size_t value = 0;
    std::string text{};
};

class OptionsPanel {
public:
    /// DISK gets the scroll arrows of MUSIK for its list of savegames.
    OptionsPanel(assets::GadgetLayout option, assets::GadgetLayout disk, assets::GadgetLayout musik,
                 assets::GadgetLayout anno, const assets::TextTable* texts);

    void set_scale(double scale);
    /// The page shown now.
    [[nodiscard]] ui::Panel& panel();
    /// Back to the first page when the mode is entered.
    void opened() noexcept;
    /// Escape: from a page back to the first (0x0040A0BA); false on the first page.
    bool back();

    void refresh(const OptionsModel& model);
    /// The mouse in pixels relative to the side panel.
    std::vector<OptionsEvent> mouse(std::int32_t x, std::int32_t y, bool down, OptionsModel& model);
    [[nodiscard]] std::optional<std::string> help(const std::vector<std::string>& leiste, const OptionsModel& model);

    /// A savegame's name is being typed (clicking a slot starts it, 0x00409910 mode 3).
    [[nodiscard]] bool editing() const noexcept { return m_editing; }
    /// Typed text (Windows-1252); Backspace, Enter and Escape as `key` 8, 13 and 27. Enter on a
    /// free slot saves the new game at once; Escape drops the name typed.
    void type(std::string_view text);
    [[nodiscard]] std::optional<OptionsEvent> key(int key);

private:
    enum class Page : std::uint8_t { Root, Disk, Musik, Anno };
    std::string text(std::string_view section, std::size_t line) const;
    void refresh_disk(const OptionsModel& model);
    void refresh_musik(const OptionsModel& model);
    void refresh_anno(const OptionsModel& model);
    void stop_editing();

    ui::Panel m_option;
    ui::Panel m_disk;
    ui::Panel m_musik;
    ui::Panel m_anno;
    const assets::TextTable* m_texts;
    Page m_page = Page::Root;
    std::size_t m_slot = 0;        ///< the selected savegame
    std::size_t m_slot_first = 0;  ///< first savegame shown
    bool m_editing = false;
    bool m_new_slot = false;       ///< the name is typed into a free slot (Enter saves)
    std::string m_name;            ///< the name being typed
    bool m_renamed = false;        ///< m_name is the new name of the selected slot (until saved)
    std::size_t m_list_first = 0;  ///< first music title shown
    std::int32_t m_humans_icon = 507;
};

}  // namespace o1602::app
