// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/balance.hpp"

#include <algorithm>

#include "o1602/core/data_file.hpp"

namespace o1602::sim {

namespace {

std::string line_of(const std::string& where, std::size_t line) {
    return where + " line " + std::to_string(line) + ": ";
}

}  // namespace

bool valid(const Balance& balance) noexcept {
    return std::all_of(kBalanceFields.begin(), kBalanceFields.end(), [&](const BalanceField& f) {
        const std::uint32_t v = balance.*f.value;
        return v >= f.min && v <= f.max;
    });
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

}  // namespace o1602::sim
