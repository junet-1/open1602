// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/text_table.hpp"

#include "o1602/assets/cod.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/core/file.hpp"

namespace o1602::assets {

TextTable TextTable::parse(std::string_view text) {
    TextTable table;
    std::vector<std::string>* current = nullptr;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.remove_suffix(1);
        }
        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
            const std::string_view name = line.substr(1, line.size() - 2);
            current = name == "END" ? nullptr : &table.m_sections[std::string(name)];
            continue;
        }
        if (current != nullptr) {
            current->emplace_back(line);
        }
        if (end == text.size()) {
            break;
        }
    }
    return table;
}

core::Result<TextTable> TextTable::load(const GameDirectory& game) {
    const auto path = game.resolve("Text.cod");
    if (!path) {
        return core::make_error(core::ErrorCode::NotFound, "Text.cod not found");
    }
    // Kept in Windows-1252 like the names in savegames (City::name).
    auto bytes = core::read_file(*path);
    if (!bytes) {
        return bytes.error();
    }
    // The History Edition's Text.cod has placeholders [[N]] for its own texts (a line "[[676]]"
    // would else start a section).
    return parse(game.expand(decode_cod(*bytes)));
}

const std::vector<std::string>& TextTable::section(std::string_view name) const {
    static const std::vector<std::string> kEmpty;
    const auto it = m_sections.find(name);
    return it == m_sections.end() ? kEmpty : it->second;
}

}  // namespace o1602::assets
