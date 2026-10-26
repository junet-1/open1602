// SPDX-License-Identifier: GPL-3.0-or-later
// The settings of the side panel (BT-2) without the original data: synthetic OPTION, DISK, MUSIK
// and ANNO layouts with one gadget per Id, clicked with the mouse as the side panel does.

#include <algorithm>
#include <string>
#include <vector>

#include "../test_framework.hpp"
#include "options_panel.hpp"

#include "o1602/assets/gad.hpp"
#include "o1602/assets/text_table.hpp"

using namespace o1602;
using app::OptionsEvent;
using app::OptionsModel;
using app::OptionsPanel;

namespace {

/// A layout of the Ids first..last but `missing`, each a 20x10 box of its own place.
assets::GadgetLayout layout(std::int32_t first, std::int32_t last, const std::vector<std::int32_t>& missing = {}) {
    std::string text = "Objekt: GADGET\n";
    for (std::int32_t id = first; id <= last; ++id) {
        if (std::find(missing.begin(), missing.end(), id) != missing.end()) {
            continue;
        }
        const std::int32_t i = id - first;
        text += "  @Nummer: " + std::to_string(i) + "\n  Id: " + std::to_string(id) +
                "\n  Kind: 6\n  Pos: " + std::to_string(i % 8 * 30) + ", " + std::to_string(i / 8 * 20) +
                "\n  Size: 20, 10\n";
    }
    return assets::parse_gad(text + "EndObj\n");
}

struct Options {
    assets::TextTable texts = assets::TextTable::parse("[GAME]\n" + std::string(40, '\n'));
    OptionsPanel panel{layout(47002, 47040), layout(48002, 48080), layout(49002, 49060),
                       layout(66001, 66060, {66028, 66038}), &texts};  // ANNO.GAD has seven voice rows

    /// A click on gadget `id` of the page shown.
    std::vector<OptionsEvent> click(std::int32_t id, OptionsModel& model) {
        const auto at = panel.panel().position(id);
        REQUIRE(at.has_value());
        const std::int32_t x = (*at)[0] + 5;
        const std::int32_t y = (*at)[1] + 5;
        std::vector<OptionsEvent> out = panel.mouse(x, y, true, model);
        for (OptionsEvent& e : panel.mouse(x, y, false, model)) {
            out.push_back(std::move(e));
        }
        return out;
    }
};

std::size_t count(const std::vector<OptionsEvent>& events, OptionsEvent::Kind kind) {
    return static_cast<std::size_t>(
        std::count_if(events.begin(), events.end(), [&](const OptionsEvent& e) { return e.kind == kind; }));
}

}  // namespace

TEST_CASE("options panel: the first page switches full screen and the mission window and opens the pages") {
    Options o;
    OptionsModel model;
    o.panel.opened();
    o.panel.refresh(model);
    auto events = o.click(47022, model);  // full screen
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == OptionsEvent::Kind::Fullscreen);
    CHECK(events[0].value == 1);
    CHECK(model.fullscreen);
    events = o.click(47021, model);  // window
    REQUIRE(events.size() == 1);
    CHECK(!model.fullscreen);
    events = o.click(47015, model);
    CHECK(count(events, OptionsEvent::Kind::MissionWindow) == 1);
    CHECK(model.mission_window);
    CHECK(count(o.click(47023, model), OptionsEvent::Kind::Display) == 1);
    CHECK(!o.panel.back());  // nothing to go back from

    const ui::Panel* first = &o.panel.panel();
    CHECK(o.click(47012, model).empty());
    CHECK(&o.panel.panel() != first);
    CHECK(o.panel.back());
    CHECK(&o.panel.panel() == first);
}

TEST_CASE("options panel: music and effects change the audio settings") {
    Options o;
    OptionsModel model;
    model.tracks = {"One", "Two"};
    o.panel.opened();
    o.panel.refresh(model);
    o.click(47012, model);
    o.panel.refresh(model);
    const std::int32_t volume = model.audio.music_volume;
    CHECK(count(o.click(49014, model), OptionsEvent::Kind::Audio) == 1);
    CHECK(model.audio.music_volume == volume + 50);
    o.click(49016, model);
    CHECK(model.audio.effect_volume == -50);
    o.click(49021, model);
    CHECK(!model.audio.random_music);
    o.click(49024, model);
    CHECK(!model.audio.music);
    CHECK(count(o.click(49023, model), OptionsEvent::Kind::TestEffects) == 1);
    // A title's box switches it off; its name plays it (and switches it on again).
    o.click(49032, model);
    CHECK(model.audio.disabled_songs == std::vector<std::string>{"Two"});
    const auto events = o.click(49052, model);
    REQUIRE(count(events, OptionsEvent::Kind::PlayTrack) == 1);
    CHECK(std::find_if(events.begin(), events.end(), [](const OptionsEvent& e) {
              return e.kind == OptionsEvent::Kind::PlayTrack && e.text == "Two";
          }) != events.end());
    CHECK(model.audio.disabled_songs.empty());
}

TEST_CASE("options panel: the savegames are named, saved, loaded and the game left") {
    Options o;
    OptionsModel model;
    model.slot_names = {"First", ""};
    model.slot_humans = {1, 0};
    o.panel.opened();
    o.panel.refresh(model);
    o.click(47011, model);
    o.panel.refresh(model);

    // A free slot: its name is typed and Enter saves at once.
    CHECK(o.click(48062, model).empty());
    CHECK(o.panel.editing());
    o.panel.type("Newx");
    CHECK(!o.panel.key(8).has_value());  // Backspace
    const auto saved = o.panel.key(13);
    REQUIRE(saved.has_value());
    CHECK(saved->kind == OptionsEvent::Kind::Save);
    CHECK(saved->value == 1);
    CHECK(saved->text == "New");
    CHECK(!o.panel.editing());

    // A used slot: a new name waits for "Sichern"; Escape drops it.
    o.click(48061, model);
    o.panel.type(" game");
    CHECK(!o.panel.key(13).has_value());
    auto events = o.click(48011, model);
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == OptionsEvent::Kind::Save);
    CHECK(events[0].value == 0);
    CHECK(events[0].text == "First game");
    o.click(48061, model);
    o.panel.type("lost");
    CHECK(!o.panel.key(27).has_value());
    events = o.click(48011, model);
    REQUIRE(events.size() == 1);
    CHECK(events[0].text == "First");

    events = o.click(48012, model);
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == OptionsEvent::Kind::Load);
    CHECK(events[0].value == 0);
    CHECK(count(o.click(48013, model), OptionsEvent::Kind::Quit) == 1);
}

TEST_CASE("options panel: the last game in the first slot loads but is never named or saved into") {
    Options o;
    OptionsModel model;
    model.slot_names = {"lastgame", "Mine", ""};
    model.slot_humans = {1, 1, 0};
    model.reserved_slots = 1;
    o.panel.opened();
    o.panel.refresh(model);
    o.click(47011, model);
    o.panel.refresh(model);

    CHECK(o.click(48061, model).empty());
    CHECK(!o.panel.editing());
    o.panel.refresh(model);
    CHECK(o.click(48011, model).empty());  // "Sichern"
    auto events = o.click(48012, model);   // "Laden"
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == OptionsEvent::Kind::Load);
    CHECK(events[0].value == 0);

    // The slots after it as before.
    o.click(48062, model);
    CHECK(o.panel.editing());
    CHECK(!o.panel.key(27).has_value());
    o.panel.refresh(model);
    events = o.click(48011, model);
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == OptionsEvent::Kind::Save);
    CHECK(events[0].value == 1);
}

TEST_CASE("options panel: voice messages and videos per kind") {
    Options o;
    OptionsModel model;
    o.panel.opened();
    o.panel.refresh(model);
    o.click(47013, model);
    o.panel.refresh(model);
    o.click(66022, model);  // the second kind
    CHECK(!model.audio.speech[2]);
    o.click(66004, model);  // all voice messages off: the kinds stay as they are
    CHECK(!model.audio.speech[0]);
    o.click(66023, model);  // and take no clicks
    CHECK(model.audio.speech[3]);
    CHECK(!model.audio.speech[2]);
    o.click(66041, model);  // kinds 1 and 2 share a box
    CHECK(!model.audio.video[1]);
    CHECK(!model.audio.video[2]);
}

TEST_CASE("options panel: the trade routes get an eighth voice row like the seven above it") {
    Options o;
    OptionsModel model;
    o.panel.opened();
    o.panel.refresh(model);
    o.click(47013, model);
    o.panel.refresh(model);
    const ui::Panel& page = o.panel.panel();
    const auto seventh = page.position(66027);
    const auto eighth = page.position(66028);
    REQUIRE(seventh.has_value());
    REQUIRE(eighth.has_value());
    CHECK((*eighth)[0] == (*seventh)[0]);
    CHECK((*eighth)[1] == (*seventh)[1] + 25);
    REQUIRE(page.position(66038).has_value());
    CHECK((*page.position(66038))[1] == (*page.position(66037))[1] + 25);
    // The video part's picture in three pieces: the voice box's edge under the eighth row.
    REQUIRE(page.position(66091).has_value());
    CHECK((*page.position(66091))[1] == (*page.position(66005))[1] + 13);
    o.click(66028, model);
    CHECK(!model.audio.speech[audio::kSpeechRoutes]);
    CHECK(model.audio.speech[7]);
    o.click(66028, model);
    CHECK(model.audio.speech[audio::kSpeechRoutes]);
}
