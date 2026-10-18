// SPDX-License-Identifier: GPL-3.0-or-later
#include "options_panel.hpp"

#include <algorithm>
#include <utility>

namespace o1602::app {

namespace {

// OPTION.GAD
constexpr std::int32_t kOptionTitle = 47002;
constexpr std::int32_t kPageDisk = 47011;
constexpr std::int32_t kPageMusik = 47012;
constexpr std::int32_t kPageAnno = 47013;
constexpr std::int32_t kMonitor = 47014;
constexpr std::int32_t kMission = 47015;
constexpr std::int32_t kResolution = 47021;  ///< + 0..2
constexpr std::int32_t kResolutionLabel = 47031;
// DISK.GAD
constexpr std::int32_t kDiskTitle = 48002;
constexpr std::int32_t kSave = 48011;
constexpr std::int32_t kLoad = 48012;
constexpr std::int32_t kQuit = 48013;
constexpr std::int32_t kSlotName = 48021;    ///< + slot
constexpr std::int32_t kSlotHumans = 48041;  ///< + slot
constexpr std::int32_t kSlotFrame = 48061;   ///< + slot
// MUSIK.GAD
constexpr std::int32_t kMusikTitle = 49002;
constexpr std::int32_t kListDown = 49011;
constexpr std::int32_t kListUp = 49012;
constexpr std::int32_t kMusicLess = 49013;
constexpr std::int32_t kMusicMore = 49014;
constexpr std::int32_t kMusicSlider = 49015;
constexpr std::int32_t kEffectsLess = 49016;
constexpr std::int32_t kEffectsMore = 49017;
constexpr std::int32_t kEffectsSlider = 49018;
constexpr std::int32_t kSequential = 49021;
constexpr std::int32_t kRandom = 49022;
constexpr std::int32_t kTest = 49023;
constexpr std::int32_t kMusicSwitch = 49024;
constexpr std::int32_t kMusicLabel = 49025;
constexpr std::int32_t kEffectsSwitch = 49026;
constexpr std::int32_t kEffectsLabel = 49027;
constexpr std::int32_t kTrackBox = 49031;    ///< + row
constexpr std::int32_t kTrackTitle = 49041;  ///< + row
constexpr std::int32_t kTrackFrame = 49051;  ///< + row
constexpr std::size_t kTrackRows = 6;
// ANNO.GAD
constexpr std::int32_t kSpeechTitle = 66003;
constexpr std::int32_t kSpeechSwitch = 66004;
constexpr std::int32_t kVideoTitle = 66007;
constexpr std::int32_t kVideoSwitch = 66008;
constexpr std::int32_t kSpeechBox = 66021;    ///< + kind - 1
constexpr std::int32_t kSpeechLabel = 66031;  ///< + kind - 1
constexpr std::int32_t kVideoBox = 66041;     ///< kinds {1, 2}, 5, 7
constexpr std::int32_t kVideoLabel = 66051;

constexpr std::int32_t kMinVolume = -4000;  ///< 1/100 dB
constexpr std::int32_t kVolumeStep = 50;

bool track_enabled(const audio::AudioSettings& a, const std::string& title) {
    return std::find(a.disabled_songs.begin(), a.disabled_songs.end(), title) == a.disabled_songs.end();
}

void set_track_enabled(audio::AudioSettings& a, const std::string& title, bool on) {
    std::erase(a.disabled_songs, title);
    if (!on) {
        a.disabled_songs.push_back(title);
    }
}

}  // namespace

OptionsPanel::OptionsPanel(assets::GadgetLayout option, assets::GadgetLayout disk, assets::GadgetLayout musik,
                           assets::GadgetLayout anno, const assets::TextTable* texts)
    : m_option(std::move(option), 1.0),
      m_disk(std::move(disk), 1.0),
      m_musik(std::move(musik), 1.0),
      m_anno(std::move(anno), 1.0),
      m_texts(texts) {
    m_option.set_text(kOptionTitle, text("GAME", 25));
    constexpr std::array<const char*, 3> kSizes = {"640x480", "800x600", "1024x768"};
    for (std::size_t i = 0; i < kSizes.size(); ++i) {
        m_option.set_text(kResolutionLabel + static_cast<std::int32_t>(i), kSizes[i]);
    }
    m_disk.set_text(kDiskTitle, text("GAME", 26));
    m_musik.set_text(kMusikTitle, text("GAME", 27));
    m_musik.set_text(kMusicLabel, text("GAME", 30));
    m_musik.set_text(kEffectsLabel, text("GAME", 31));
    m_anno.set_text(kSpeechTitle, text("GAME", 28));
    m_anno.set_text(kVideoTitle, text("GAME", 29));
    for (std::size_t k = 0; k < 7; ++k) {
        m_anno.set_text(kSpeechLabel + static_cast<std::int32_t>(k), text("SPEECHKIND", k));
    }
    for (std::size_t k = 0; k < 3; ++k) {
        m_anno.set_text(kVideoLabel + static_cast<std::int32_t>(k), text("VIDEOKIND", k));
    }
    const assets::GadgetLayout& l = m_disk.layout();
    if (const auto g = l.index_of(kSlotHumans)) {
        m_humans_icon = l.gadgets[*g].gfx;
    }
}

std::string OptionsPanel::text(std::string_view section, std::size_t line) const {
    if (m_texts == nullptr) {
        return {};
    }
    const auto& lines = m_texts->section(std::string(section));
    return line < lines.size() ? lines[line] : std::string();
}

void OptionsPanel::set_scale(double scale) {
    for (ui::Panel* p : {&m_option, &m_disk, &m_musik, &m_anno}) {
        p->set_scale(scale);
    }
}

ui::Panel& OptionsPanel::panel() {
    switch (m_page) {
        case Page::Disk:
            return m_disk;
        case Page::Musik:
            return m_musik;
        case Page::Anno:
            return m_anno;
        case Page::Root:
            break;
    }
    return m_option;
}

void OptionsPanel::opened() noexcept {
    m_page = Page::Root;
    stop_editing();
    m_renamed = false;
}

bool OptionsPanel::back() {
    if (m_page == Page::Root) {
        return false;
    }
    stop_editing();
    m_renamed = false;
    m_page = Page::Root;
    return true;
}

void OptionsPanel::stop_editing() {
    m_editing = false;
}

void OptionsPanel::refresh(const OptionsModel& model) {
    switch (m_page) {
        case Page::Root:
            // The resolutions are not offered in a game of several players (0x00439630).
            for (std::size_t i = 0; i < 3; ++i) {
                const auto k = static_cast<std::int32_t>(i);
                m_option.set_visible(kResolution + k, !model.multiplayer);
                m_option.set_visible(kResolutionLabel + k, !model.multiplayer);
                m_option.set_selected(kResolution + k, i == model.resolution);
            }
            m_option.set_visible(kMonitor, !model.multiplayer);
            m_option.set_selected(kMission, model.mission_window);
            break;
        case Page::Disk:
            refresh_disk(model);
            break;
        case Page::Musik:
            refresh_musik(model);
            break;
        case Page::Anno:
            refresh_anno(model);
            break;
    }
}

void OptionsPanel::refresh_disk(const OptionsModel& model) {
    for (std::size_t i = 0; i < OptionsModel::kSlots; ++i) {
        const auto k = static_cast<std::int32_t>(i);
        // The name being typed shows with the cursor; it is kept only when saved (U-037).
        const bool typing = m_editing && i == m_slot;
        const bool renamed = m_renamed && i == m_slot;
        m_disk.set_text(kSlotName + k, typing ? m_name + "_" : renamed ? m_name : model.slot_names[i]);
        const std::uint8_t humans = model.slot_humans[i];
        m_disk.set_visible(kSlotHumans + k, humans >= 2);
        if (humans >= 2) {
            m_disk.set_graphic(kSlotHumans + k, m_humans_icon - 1 + std::min<std::int32_t>(humans, 4), 0);
        }
        m_disk.set_selected(kSlotFrame + k, i == m_slot);
        m_disk.set_hidden(kSlotFrame + k, i != m_slot);
    }
    // A client of a game of several players cannot load (0x00439630).
    m_disk.set_visible(kLoad, !model.multiplayer);
}

void OptionsPanel::refresh_musik(const OptionsModel& model) {
    const audio::AudioSettings& a = model.audio;
    const std::size_t count = model.tracks.size();
    m_list_first = std::min(m_list_first, count > kTrackRows ? count - kTrackRows : 0);
    for (std::size_t r = 0; r < kTrackRows; ++r) {
        const auto k = static_cast<std::int32_t>(r);
        const std::size_t t = m_list_first + r;
        const bool shown = t < count;
        m_musik.set_visible(kTrackBox + k, shown);
        m_musik.set_visible(kTrackTitle + k, shown);
        m_musik.set_visible(kTrackFrame + k, shown);
        if (!shown) {
            continue;
        }
        m_musik.set_text(kTrackTitle + k, model.tracks[t]);
        m_musik.set_selected(kTrackBox + k, track_enabled(a, model.tracks[t]));
        m_musik.set_hidden(kTrackFrame + k, model.tracks[t] != model.playing);
    }
    // The original hides the up arrow at the top but only greys the down arrow at the bottom (M7);
    // Open1602 greys both.
    m_musik.set_disabled(kListUp, m_list_first == 0);
    m_musik.set_disabled(kListDown, m_list_first + kTrackRows >= count);
    if (m_musik.held() != kMusicSlider) {
        m_musik.set_slider(kMusicSlider, 0, a.music_volume - kMinVolume, -kMinVolume);
    }
    if (m_musik.held() != kEffectsSlider) {
        m_musik.set_slider(kEffectsSlider, 0, a.effect_volume - kMinVolume, -kMinVolume);
    }
    m_musik.set_selected(kSequential, !a.random_music);
    m_musik.set_selected(kRandom, a.random_music);
    m_musik.set_selected(kMusicSwitch, !a.music);  // crossed out: off
    m_musik.set_selected(kEffectsSwitch, !a.effects);
}

void OptionsPanel::refresh_anno(const OptionsModel& model) {
    const audio::AudioSettings& a = model.audio;
    m_anno.set_selected(kSpeechSwitch, !a.speech[0]);  // crossed out: all off
    for (std::size_t k = 1; k < a.speech.size(); ++k) {
        const auto id = kSpeechBox + static_cast<std::int32_t>(k - 1);
        m_anno.set_selected(id, a.speech[k]);
        m_anno.set_disabled(id, !a.speech[0]);  // drawn faded while all are off
    }
    m_anno.set_selected(kVideoSwitch, !a.video[0]);
    m_anno.set_selected(kVideoBox, a.video[1] || a.video[2]);
    m_anno.set_selected(kVideoBox + 1, a.video[5]);
    m_anno.set_selected(kVideoBox + 2, a.video[7]);
    for (std::int32_t i = 0; i < 3; ++i) {
        m_anno.set_disabled(kVideoBox + i, !a.video[0]);
    }
}

std::vector<OptionsEvent> OptionsPanel::mouse(std::int32_t x, std::int32_t y, bool down, OptionsModel& model) {
    std::vector<OptionsEvent> out;
    audio::AudioSettings& a = model.audio;
    const auto audio_changed = [&] { out.push_back(OptionsEvent{OptionsEvent::Kind::Audio}); };
    for (const ui::PanelEvent& e : panel().mouse(x, y, down)) {
        if (e.kind == ui::PanelEvent::Kind::Slid) {
            if (e.id == kMusicSlider) {
                a.music_volume = m_musik.slider(kMusicSlider, 0, -kMinVolume) + kMinVolume;
                audio_changed();
            } else if (e.id == kEffectsSlider) {
                a.effect_volume = m_musik.slider(kEffectsSlider, 0, -kMinVolume) + kMinVolume;
                audio_changed();
            }
            continue;
        }
        if (e.kind != ui::PanelEvent::Kind::Clicked) {
            continue;
        }
        switch (m_page) {
            case Page::Root:
                if (e.id == kPageDisk) {
                    m_page = Page::Disk;
                } else if (e.id == kPageMusik) {
                    m_page = Page::Musik;
                } else if (e.id == kPageAnno) {
                    m_page = Page::Anno;
                } else if (e.id == kMission) {
                    model.mission_window = !model.mission_window;
                    out.push_back(OptionsEvent{OptionsEvent::Kind::MissionWindow});
                } else if (e.id >= kResolution && e.id < kResolution + 3) {
                    out.push_back(
                        OptionsEvent{OptionsEvent::Kind::Resolution, static_cast<std::size_t>(e.id - kResolution)});
                }
                break;
            case Page::Disk:
                if (e.id >= kSlotFrame && e.id < kSlotFrame + static_cast<std::int32_t>(OptionsModel::kSlots)) {
                    // Choosing a slot starts typing its name (0x0043F7F4).
                    m_slot = static_cast<std::size_t>(e.id - kSlotFrame);
                    m_name = model.slot_names[m_slot];
                    m_editing = true;
                    m_renamed = true;
                } else if (e.id == kSave) {
                    const std::string name = m_renamed ? m_name : model.slot_names[m_slot];
                    stop_editing();
                    m_renamed = false;
                    out.push_back(OptionsEvent{OptionsEvent::Kind::Save, m_slot, name});
                } else if (e.id == kLoad) {
                    stop_editing();
                    m_renamed = false;  // the name typed is not kept without saving (U-037)
                    out.push_back(OptionsEvent{OptionsEvent::Kind::Load, m_slot});
                } else if (e.id == kQuit) {
                    stop_editing();
                    out.push_back(OptionsEvent{OptionsEvent::Kind::Quit});
                }
                break;
            case Page::Musik:
                if (e.id == kListDown && m_list_first + kTrackRows < model.tracks.size()) {
                    ++m_list_first;
                } else if (e.id == kListUp && m_list_first > 0) {
                    --m_list_first;
                } else if (e.id == kMusicLess || e.id == kMusicMore) {
                    a.music_volume =
                        std::clamp(a.music_volume + (e.id == kMusicMore ? kVolumeStep : -kVolumeStep), kMinVolume, 0);
                    audio_changed();
                } else if (e.id == kEffectsLess || e.id == kEffectsMore) {
                    a.effect_volume = std::clamp(a.effect_volume + (e.id == kEffectsMore ? kVolumeStep : -kVolumeStep),
                                                 kMinVolume, 0);
                    audio_changed();
                } else if (e.id == kSequential || e.id == kRandom) {
                    a.random_music = e.id == kRandom;
                    audio_changed();
                } else if (e.id == kTest) {
                    out.push_back(OptionsEvent{OptionsEvent::Kind::TestEffects});
                } else if (e.id == kMusicSwitch) {
                    a.music = !a.music;
                    audio_changed();
                } else if (e.id == kEffectsSwitch) {
                    a.effects = !a.effects;
                    audio_changed();
                } else if (e.id >= kTrackBox && e.id < kTrackBox + static_cast<std::int32_t>(kTrackRows)) {
                    const std::size_t t = m_list_first + static_cast<std::size_t>(e.id - kTrackBox);
                    if (t < model.tracks.size()) {
                        set_track_enabled(a, model.tracks[t], !track_enabled(a, model.tracks[t]));
                        audio_changed();
                    }
                } else if (e.id >= kTrackFrame && e.id < kTrackFrame + static_cast<std::int32_t>(kTrackRows)) {
                    const std::size_t t = m_list_first + static_cast<std::size_t>(e.id - kTrackFrame);
                    if (t < model.tracks.size()) {
                        set_track_enabled(a, model.tracks[t], true);
                        audio_changed();
                        out.push_back(OptionsEvent{OptionsEvent::Kind::PlayTrack, 0, model.tracks[t]});
                    }
                }
                break;
            case Page::Anno:
                if (e.id == kSpeechSwitch) {
                    a.speech[0] = !a.speech[0];
                    audio_changed();
                } else if (e.id >= kSpeechBox && e.id < kSpeechBox + 7 && a.speech[0]) {
                    const auto k = static_cast<std::size_t>(e.id - kSpeechBox + 1);
                    a.speech[k] = !a.speech[k];
                    audio_changed();
                } else if (e.id == kVideoSwitch) {
                    a.video[0] = !a.video[0];
                    audio_changed();
                } else if (e.id == kVideoBox && a.video[0]) {
                    const bool on = !(a.video[1] || a.video[2]);
                    a.video[1] = on;  // kinds 1 and 2 share the box
                    a.video[2] = on;
                    audio_changed();
                } else if ((e.id == kVideoBox + 1 || e.id == kVideoBox + 2) && a.video[0]) {
                    const std::size_t k = e.id == kVideoBox + 1 ? 5 : 7;
                    a.video[k] = !a.video[k];
                    audio_changed();
                }
                break;
        }
    }
    return out;
}

std::optional<std::string> OptionsPanel::help(const std::vector<std::string>& leiste, const OptionsModel& model) {
    const auto id = panel().hovered();
    if (!id) {
        return std::nullopt;
    }
    std::optional<std::size_t> line;
    const audio::AudioSettings& a = model.audio;
    switch (m_page) {
        case Page::Root:
            if (*id == kPageDisk) {
                line = 177;
            } else if (*id == kPageMusik) {
                line = 178;
            } else if (*id == kPageAnno) {
                line = 179;
            } else if (*id == kMission) {
                line = 181;
            } else if (*id >= kResolution && *id < kResolution + 3) {
                line = 180;
            }
            break;
        case Page::Disk:
            if (*id == kSave) {
                line = 173;
            } else if (*id == kLoad) {
                line = 172;
            } else if (*id == kQuit) {
                line = 174;
            } else if (*id >= kSlotHumans && *id < kSlotHumans + 12) {
                line = 176;
            } else if (*id >= kSlotFrame && *id < kSlotFrame + 12) {
                line = 175;
            }
            break;
        case Page::Musik:
            if (*id == kListDown || *id == kListUp) {
                line = 182;
            } else if (*id >= kMusicLess && *id <= kEffectsSlider) {
                line = 183 + static_cast<std::size_t>(*id - kMusicLess);
            } else if (*id == kSequential) {
                line = 189;
            } else if (*id == kRandom) {
                line = 190;
            } else if (*id == kTest) {
                line = 191;
            } else if (*id == kMusicSwitch) {
                line = a.music ? 192 : 193;
            } else if (*id == kEffectsSwitch) {
                line = a.effects ? 194 : 195;
            } else if (*id >= kTrackBox && *id < kTrackBox + static_cast<std::int32_t>(kTrackRows)) {
                const std::size_t t = m_list_first + static_cast<std::size_t>(*id - kTrackBox);
                line = t < model.tracks.size() && track_enabled(a, model.tracks[t]) ? 197 : 198;
            } else if (*id >= kTrackFrame && *id < kTrackFrame + static_cast<std::int32_t>(kTrackRows)) {
                line = 196;
            }
            break;
        case Page::Anno:
            if (*id == kSpeechSwitch) {
                line = a.speech[0] ? 236 : 237;
            } else if (*id >= kSpeechBox && *id < kSpeechBox + 7) {
                line = a.speech[static_cast<std::size_t>(*id - kSpeechBox + 1)] ? 234 : 235;
            } else if (*id == kVideoSwitch) {
                line = a.video[0] ? 230 : 231;
            } else if (*id >= kVideoBox && *id < kVideoBox + 3) {
                line = m_anno.selected(*id) ? 228 : 229;
            }
            break;
    }
    if (!line || *line >= leiste.size()) {
        return std::nullopt;
    }
    return leiste[*line];
}

void OptionsPanel::type(std::string_view text) {
    if (!m_editing) {
        return;
    }
    constexpr std::size_t kMaxName = 18;  // the original's limit (0x00409910 with 0x13)
    for (const char c : text) {
        if (static_cast<unsigned char>(c) >= 0x20 && m_name.size() < kMaxName) {
            m_name.push_back(c);
        }
    }
}

void OptionsPanel::key(int key) {
    if (!m_editing) {
        return;
    }
    if (key == 8 && !m_name.empty()) {
        m_name.pop_back();
    } else if (key == 13) {
        m_editing = false;  // the name waits for "Sichern"
    }
}

}  // namespace o1602::app
