// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/high_scores.hpp"

#include <algorithm>
#include <charconv>

namespace o1602::assets {

namespace {

constexpr std::uint32_t kNoCampaign = 0xFF;

/// The numbers of a line after its key, separated by blanks.
std::vector<std::int64_t> numbers(std::string_view s) {
    std::vector<std::int64_t> out;
    std::size_t pos = 0;
    while (pos < s.size()) {
        const std::size_t a = s.find_first_of("-0123456789", pos);
        if (a == std::string_view::npos) {
            break;
        }
        std::int64_t v = 0;
        const auto [end, ec] = std::from_chars(s.data() + a, s.data() + s.size(), v);
        if (ec != std::errc{}) {
            break;
        }
        out.push_back(v);
        pos = static_cast<std::size_t>(end - s.data());
    }
    return out;
}

}  // namespace

std::string write_high_scores(const std::vector<HighScore>& scores) {
    std::string out = "\n  Objekt:   HISCORE\n\n";
    for (std::size_t i = 0; i < std::max(scores.size(), kHighScoreLines); ++i) {
        const HighScore h = i < scores.size() ? scores[i] : HighScore{};
        std::string name = h.name;
        std::replace(name.begin(), name.end(), '"', '\'');  // the reader stops at the next quote
        out += "    Entry:    " + std::to_string(i) + ", \"" + name + "\", " + std::to_string(h.points) + ", " +
               std::to_string(h.stars) + ", " + std::to_string(h.game_id) + "\n";
    }
    out += "  EndObj;\n";
    return out;
}

std::optional<std::size_t> insert_high_score(std::vector<HighScore>& scores, const HighScore& entry) {
    std::erase_if(scores, [](const HighScore& h) { return h.name.empty(); });
    const auto same = std::find_if(scores.begin(), scores.end(), [&](const HighScore& h) {
        return entry.game_id != 0 && h.game_id == entry.game_id && h.name == entry.name;
    });
    if (same != scores.end()) {
        if (same->points >= entry.points) {
            return static_cast<std::size_t>(same - scores.begin());
        }
        scores.erase(same);
    }
    const auto at =
        std::find_if(scores.begin(), scores.end(), [&](const HighScore& h) { return h.points < entry.points; });
    const auto index = static_cast<std::size_t>(at - scores.begin());
    if (index >= kHighScoreLines) {
        return std::nullopt;
    }
    scores.insert(at, entry);
    if (scores.size() > kHighScoreLines) {
        scores.resize(kHighScoreLines);
    }
    return index;
}

bool CampaignProgress::record_win(std::uint32_t campaign, std::uint32_t part, std::uint32_t mission_number) {
    bool changed = false;
    if (campaign != kNoCampaign) {
        std::uint32_t& next = campaigns[campaign];
        if (part + 1 > next) {
            next = part + 1;
            changed = true;
        }
    } else if (mission_number >= kFirstNumbered && mission_number <= kLastNumbered && numbered <= mission_number) {
        const std::uint32_t next = std::min(mission_number + 1, kLastNumbered);
        changed = next != numbered;
        numbered = next;
    }
    return changed;
}

CampaignProgress parse_progress(std::string_view text) {
    CampaignProgress p;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        const std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string_view::npos || line[first] == '#') {
            continue;
        }
        line.remove_prefix(first);
        const std::vector<std::int64_t> v = numbers(line);
        const auto valid = [](std::int64_t x) { return x >= 0 && x <= 0xFFFF; };
        if (line.starts_with("campaign") && v.size() >= 2 && valid(v[0]) && valid(v[1])) {
            p.campaigns[static_cast<std::uint32_t>(v[0])] = static_cast<std::uint32_t>(v[1]);
        } else if (line.starts_with("numbered") && !v.empty()) {
            p.numbered = static_cast<std::uint32_t>(
                std::clamp<std::int64_t>(v[0], CampaignProgress::kFirstNumbered, CampaignProgress::kLastNumbered));
        }
    }
    return p;
}

std::string write_progress(const CampaignProgress& progress) {
    std::string out = "# Open1602: missions opened by won games (the original keeps this in Game.dat)\n";
    out += "numbered " + std::to_string(progress.numbered) + "\n";
    for (const auto& [campaign, next] : progress.campaigns) {
        out += "campaign " + std::to_string(campaign) + " " + std::to_string(next) + "\n";
    }
    return out;
}

}  // namespace o1602::assets
