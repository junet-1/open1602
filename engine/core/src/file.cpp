// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/core/file.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <system_error>

namespace o1602::core {

namespace fs = std::filesystem;

Result<Bytes> read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return make_error(ErrorCode::NotFound, "cannot open file: " + path.string());
    }
    in.seekg(0, std::ios::end);
    const auto end = in.tellg();
    if (end < 0) {
        return make_error(ErrorCode::IoError, "cannot determine size: " + path.string());
    }
    in.seekg(0, std::ios::beg);
    Bytes data(static_cast<std::size_t>(end));
    if (!data.empty() && !in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        return make_error(ErrorCode::IoError, "cannot read file: " + path.string());
    }
    return data;
}

Status write_file(const fs::path& path, std::span<const std::uint8_t> data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return make_error(ErrorCode::IoError, "cannot create file: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!out) {
        return make_error(ErrorCode::IoError, "cannot write file: " + path.string());
    }
    return Ok{};
}

namespace {

bool iequals(std::string_view a, std::string_view b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

}  // namespace

std::optional<fs::path> find_case_insensitive(const fs::path& root, std::string_view relative) {
    std::string normalized(relative);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    std::error_code ec;
    if (fs::path exact = root / normalized; fs::exists(exact, ec)) {
        return exact;
    }

    fs::path current = root;
    std::size_t start = 0;
    while (start <= normalized.size()) {
        const std::size_t end = std::min(normalized.find('/', start), normalized.size());
        const std::string_view component(normalized.data() + start, end - start);
        start = end + 1;
        if (component.empty() || component == ".") {
            continue;
        }

        bool found = false;
        for (fs::directory_iterator it(current, ec), last; !ec && it != last; it.increment(ec)) {
            if (iequals(it->path().filename().string(), component)) {
                current = it->path();
                found = true;
                break;
            }
        }
        if (!found) {
            return std::nullopt;
        }
    }
    return current;
}

}  // namespace o1602::core
