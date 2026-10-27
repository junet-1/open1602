// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/package_graphics.hpp"

#include <algorithm>

#include "o1602/assets/game_content.hpp"
#include "o1602/core/data_file.hpp"
#include "o1602/core/file.hpp"
#include "o1602/core/png_reader.hpp"

namespace o1602::assets {

namespace {

std::string line_of(const std::string& where, std::size_t line) {
    return where + " line " + std::to_string(line) + ": ";
}

/// A name a package may give its image: lower case letters, digits and '_'.
bool is_name(std::string_view name) noexcept {
    return !name.empty() && name.size() <= 64 && std::all_of(name.begin(), name.end(), [](const char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

/// The width and height of a PNG file from its header.
core::Result<core::PngImage> png_header(const std::filesystem::path& path) {
    auto head = core::read_file_head(path, 64);
    if (!head) {
        return head.error();
    }
    return core::png_size(head->bytes);
}

}  // namespace

std::uint32_t package_sprite_width(const PackageImage& image, std::size_t zoom) noexcept {
    return (image.width + (1U << zoom) - 1) >> zoom;
}

std::uint32_t package_sprite_height(const PackageImage& image, std::size_t zoom) noexcept {
    return (image.height + (1U << zoom) - 1) >> zoom;
}

const PackageImage* find_package_image(std::span<const PackageImage> images, std::string_view id) noexcept {
    const auto it = std::find_if(images.begin(), images.end(), [&](const PackageImage& i) { return i.id == id; });
    return it == images.end() ? nullptr : &*it;
}

void apply_graphics_file(std::vector<PackageImage>& images, std::string_view package,
                         const std::filesystem::path& folder, std::string_view utf8,
                         std::vector<std::string>& warnings) {
    const std::string where = std::string(package) + "/" + std::string(kGraphicsFile);
    auto tables = core::parse_data_file(utf8);
    if (!tables) {
        warnings.push_back(where + " " + tables.error().message);
        return;
    }
    for (const core::DataTable& t : *tables) {
        const std::string at = line_of(where, t.line);
        if (!is_name(t.name)) {
            warnings.push_back(at + "[" + t.name + "]: an image's name holds only lower case letters, digits and '_'");
            continue;
        }
        PackageImage image;
        image.id = std::string(package) + ":" + t.name;
        if (find_package_image(images, image.id) != nullptr) {
            warnings.push_back(at + image.id + " is declared twice; the second is passed over");
            continue;
        }
        std::optional<std::int64_t> count;
        bool ok = true;
        for (const core::DataEntry& e : t.entries) {
            const std::string field = line_of(where, e.value.line) + "'" + e.key + "' ";
            if (e.key == "image" || e.key == "image_medium" || e.key == "image_small") {
                const std::string* s = e.value.string();
                if (s == nullptr || !safe_package_path(*s)) {
                    warnings.push_back(field + "is the path of a PNG file in the package, folders split by '/'");
                    ok = false;
                    continue;
                }
                const std::size_t zoom = e.key == "image" ? 0 : e.key == "image_medium" ? 1 : 2;
                image.files[zoom] = folder / core::path_from_utf8(*s);
            } else if (e.key == "width" || e.key == "height" || e.key == "count") {
                const std::int64_t* n = e.value.integer();
                const std::int64_t max =
                    e.key == "count" ? kEndPackageSprite - kFirstPackageSprite : std::int64_t{kMaxPackageSpriteSide};
                if (n == nullptr || *n < 1 || *n > max) {
                    warnings.push_back(field + "is a number from 1 to " + std::to_string(max));
                    ok = false;
                } else if (e.key == "count") {
                    count = *n;
                } else {
                    (e.key == "width" ? image.width : image.height) = static_cast<std::uint32_t>(*n);
                }
            } else {
                warnings.push_back(field + "is no field of an image and is passed over");
            }
        }
        if (!ok) {
            continue;
        }
        if (image.files[0].empty() || image.width == 0 || image.height == 0) {
            warnings.push_back(at + image.id + " needs an 'image', a 'width' and a 'height'");
            continue;
        }
        // The sprites are the whole cells of the grid, or the first `count` of them.
        bool readable = true;
        std::uint32_t cells = 0;
        for (std::size_t zoom = 0; zoom < kPackageZoomLevels && readable; ++zoom) {
            if (image.files[zoom].empty()) {
                continue;
            }
            std::string file = at;  // "<where> line <n>: <id>: <file>"
            file += image.id;
            file += ": ";
            file += core::path_to_utf8(image.files[zoom].lexically_relative(folder));
            const auto size = png_header(image.files[zoom]);
            if (!size) {
                file += ": ";
                file += size.error().message;
                warnings.push_back(std::move(file));
                readable = false;
                break;
            }
            const std::uint32_t w = package_sprite_width(image, zoom);
            const std::uint32_t h = package_sprite_height(image, zoom);
            const std::uint32_t here = (size->width / w) * (size->height / h);
            if (zoom == 0) {
                cells = here;
            } else if (here < cells) {
                file += " holds ";
                file += std::to_string(here);
                file += " sprites of ";
                file += std::to_string(w);
                file += " x ";
                file += std::to_string(h);
                file += ", the large image ";
                file += std::to_string(cells);
                warnings.push_back(std::move(file));
                readable = false;
            }
        }
        if (!readable) {
            continue;
        }
        if (count && *count > cells) {
            warnings.push_back(at + image.id + ": 'count' is " + std::to_string(*count) + ", the image holds " +
                               std::to_string(cells) + " sprites of " + std::to_string(image.width) + " x " +
                               std::to_string(image.height));
            continue;
        }
        image.count = count ? static_cast<std::uint32_t>(*count) : cells;
        if (image.count == 0) {
            warnings.push_back(at + image.id + ": the image is smaller than one sprite");
            continue;
        }
        image.first = images.empty() ? kFirstPackageSprite : images.back().first + images.back().count;
        if (std::uint64_t{image.first} + image.count > kEndPackageSprite) {
            warnings.push_back(at + image.id + ": the packages' images hold at most " +
                               std::to_string(kEndPackageSprite - kFirstPackageSprite) +
                               " sprites in all; it is passed over");
            continue;
        }
        images.push_back(std::move(image));
    }
}

}  // namespace o1602::assets
