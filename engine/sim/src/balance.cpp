// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/balance.hpp"

#include <algorithm>
#include <charconv>
#include <optional>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/game_data.hpp"
#include "o1602/core/data_file.hpp"
#include "o1602/core/embedded_file.hpp"
#include "o1602/sim/rules.hpp"

namespace o1602::sim {

// data/balance/*.toml, compiled in by o1602_embed_files (engine/sim/CMakeLists.txt).
namespace presets {
extern const core::EmbeddedFile kEmbeddedFiles[];
extern const std::size_t kEmbeddedFileCount;
}  // namespace presets

namespace {

/// The package whose buildings a table ["<package>:<Id>"] may change: the original's.
constexpr std::string_view kBasePackage = "anno1602";

std::string line_of(const std::string& where, std::size_t line) {
    return where + " line " + std::to_string(line) + ": ";
}

bool before(const BuildingValue& a, const BuildingValue& b) noexcept {
    return a.id != b.id ? a.id < b.id : a.field < b.field;
}

/// The Id of a table name "anno1602:<Id>" (decimal or 0x hex).
std::optional<std::uint32_t> building_id(std::string_view number) {
    int base = 10;
    if (number.size() > 2 && number[0] == '0' && (number[1] == 'x' || number[1] == 'X')) {
        number.remove_prefix(2);
        base = 16;
    }
    std::uint32_t id = 0;
    const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), id, base);
    if (error != std::errc{} || end != number.data() + number.size() || number.empty()) {
        return std::nullopt;
    }
    return id;
}

void apply_building_table(Balance& balance, const core::DataTable& table, const std::string& where,
                          std::vector<std::string>& warnings) {
    const std::size_t colon = table.name.find(':');
    const std::string_view package = std::string_view(table.name).substr(0, colon);
    const auto id = building_id(std::string_view(table.name).substr(colon + 1));
    if (package != kBasePackage) {
        // Buildings of a package's own come with the content registry (docs/modding/open-work.md).
        warnings.push_back(line_of(where, table.line) + "[" + table.name +
                           "]: only the original's buildings (anno1602:<Id>) can be changed");
        return;
    }
    if (!id) {
        warnings.push_back(line_of(where, table.line) + "[" + table.name + "]: no building Id after 'anno1602:'");
        return;
    }
    for (const core::DataEntry& e : table.entries) {
        const std::string at = line_of(where, e.value.line);
        const auto field = std::find_if(kBuildingFields.begin(), kBuildingFields.end(),
                                        [&](const BuildingFieldInfo& f) { return f.key == e.key; });
        const std::int64_t* n = e.value.integer();
        if (field == kBuildingFields.end()) {
            warnings.push_back(at + "unknown field '" + e.key + "'");
        } else if (n == nullptr || *n < field->min || *n > field->max) {
            warnings.push_back(at + "'" + e.key + "' must be a number from " + std::to_string(field->min) + " to " +
                               std::to_string(field->max));
        } else {
            set_building_value(balance, BuildingValue{*id, field->field, static_cast<std::uint32_t>(*n)});
        }
    }
}

const BuildingFieldInfo* field_info(BuildingField field) noexcept {
    const auto i = static_cast<std::size_t>(field);
    return i < kBuildingFields.size() ? &kBuildingFields[i] : nullptr;
}

}  // namespace

bool valid(const Balance& balance) noexcept {
    const bool numbers = std::all_of(kBalanceFields.begin(), kBalanceFields.end(), [&](const BalanceField& f) {
        const std::uint32_t v = balance.*f.value;
        return v >= f.min && v <= f.max;
    });
    const bool buildings = std::all_of(balance.buildings.begin(), balance.buildings.end(), [](const BuildingValue& v) {
        const BuildingFieldInfo* f = field_info(v.field);
        return f != nullptr && v.value >= f->min && v.value <= f->max;
    });
    const bool ordered = std::adjacent_find(balance.buildings.begin(), balance.buildings.end(),
                                            [](const BuildingValue& a, const BuildingValue& b) {
                                                return !before(a, b);
                                            }) == balance.buildings.end();
    return numbers && buildings && ordered;
}

bool original_numbers(const Balance& balance) noexcept {
    const Balance original;
    return std::all_of(kBalanceFields.begin(), kBalanceFields.end(),
                       [&](const BalanceField& f) { return balance.*f.value == original.*f.value; });
}

void set_building_value(Balance& balance, BuildingValue value) {
    auto& values = balance.buildings;
    const auto at = std::lower_bound(values.begin(), values.end(), value, before);
    if (at != values.end() && at->id == value.id && at->field == value.field) {
        at->value = value.value;
    } else {
        values.insert(at, value);
    }
}

void add_building_values(Balance& balance, const std::vector<BuildingValue>& under) {
    for (const BuildingValue& v : under) {
        const auto at = std::lower_bound(balance.buildings.begin(), balance.buildings.end(), v, before);
        if (at == balance.buildings.end() || at->id != v.id || at->field != v.field) {
            balance.buildings.insert(at, v);
        }
    }
}

void apply_balance_file(Balance& balance, std::string_view package, std::string_view utf8,
                        std::vector<std::string>& warnings) {
    const std::string where = std::string(package) + "/" + std::string(kBalanceFile);
    auto tables = core::parse_data_file(utf8);
    if (!tables) {
        warnings.push_back(where + " " + tables.error().message);
        return;
    }
    for (const core::DataTable& table : *tables) {
        if (table.name.empty()) {
            warnings.push_back(line_of(where, table.entries.front().value.line) +
                               "fields before the first [table] belong to none and are passed over");
            continue;
        }
        if (table.name.find(':') != std::string::npos) {
            apply_building_table(balance, table, where, warnings);
            continue;
        }
        if (std::none_of(kBalanceFields.begin(), kBalanceFields.end(),
                         [&](const BalanceField& f) { return f.table == table.name; })) {
            warnings.push_back(line_of(where, table.line) + "there is no table [" + table.name + "]");
            continue;
        }
        for (const core::DataEntry& e : table.entries) {
            const std::string at = line_of(where, e.value.line);
            const auto field = std::find_if(kBalanceFields.begin(), kBalanceFields.end(), [&](const BalanceField& f) {
                return f.table == table.name && f.key == e.key;
            });
            const std::int64_t* n = e.value.integer();
            if (field == kBalanceFields.end()) {
                warnings.push_back(at + "unknown field '" + e.key + "'");
            } else if (n == nullptr || *n < field->min || *n > field->max) {
                warnings.push_back(at + "'" + e.key + "' must be a number from " + std::to_string(field->min) + " to " +
                                   std::to_string(field->max));
            } else {
                balance.*field->value = static_cast<std::uint32_t>(*n);
            }
        }
    }
}

void drop_unknown_buildings(Balance& balance, const assets::BuildingTable& buildings,
                            std::vector<std::string>& warnings) {
    std::erase_if(balance.buildings, [&](const BuildingValue& v) {
        if (buildings.by_id(v.id) != nullptr) {
            return false;
        }
        warnings.push_back(std::string(kBalanceFile) + ": haeuser.cod has no building anno1602:" +
                           std::to_string(v.id) + ", its values are passed over");
        return true;
    });
}

void apply_building_values(assets::BuildingTable& table, const assets::BuildingTable& original,
                           const Balance& balance) {
    const std::vector<assets::BuildingType>& from = original.types();
    std::span<assets::BuildingType> types = table.changeable_types();
    for (std::size_t i = 0; i < types.size() && i < from.size(); ++i) {
        assets::BuildingType& t = types[i];
        const assets::BuildingType& o = from[i];
        t.produce_amount = o.produce_amount;
        t.raw_amount = o.raw_amount;
        t.raw2_amount = o.raw2_amount;
        t.interval = o.interval;
        t.max_stock = o.max_stock;
        t.running_cost = o.running_cost;
        t.idle_cost = o.idle_cost;
        t.build_cost.money = o.build_cost.money;
        t.build_cost.tools = o.build_cost.tools;
        t.build_cost.wood = o.build_cost.wood;
        t.build_cost.bricks = o.build_cost.bricks;
    }
    constexpr std::uint32_t kTon = assets::kStockUnitsPerTon;
    for (const BuildingValue& v : balance.buildings) {
        assets::BuildingType* t = table.by_id(v.id);
        if (t == nullptr) {
            continue;
        }
        switch (v.field) {
            case BuildingField::Produce:
                t->produce_amount = v.value * kTon;
                break;
            case BuildingField::Raw:
                t->raw_amount = v.value * kTon;
                break;
            case BuildingField::Raw2:
                t->raw2_amount = v.value * kTon;
                break;
            case BuildingField::Interval:
                t->interval = v.value;
                break;
            case BuildingField::MaxStock:
                t->max_stock = v.value * kTon;
                break;
            case BuildingField::RunningCost:
                t->running_cost = v.value;
                break;
            case BuildingField::IdleCost:
                t->idle_cost = v.value;
                break;
            case BuildingField::BuildMoney:
                t->build_cost.money = static_cast<std::int32_t>(v.value);
                break;
            case BuildingField::BuildTools:
                t->build_cost.tools = v.value * kTon;
                break;
            case BuildingField::BuildWood:
                t->build_cost.wood = v.value * kTon;
                break;
            case BuildingField::BuildBricks:
                t->build_cost.bricks = v.value * kTon;
                break;
        }
    }
}

std::vector<BuildingValue> rule_building_values(const Rules& rules) {
    Balance balance;
    std::vector<std::string> warnings;
    const auto add = [&](std::string_view name) {
        for (std::size_t i = 0; i < presets::kEmbeddedFileCount; ++i) {
            if (presets::kEmbeddedFiles[i].name == name) {
                apply_balance_file(balance, "open1602", presets::kEmbeddedFiles[i].text(), warnings);
            }
        }
    };
    if (rules.revised_bread) {
        add("revised_bread");
    }
    return balance.buildings;
}

}  // namespace o1602::sim
