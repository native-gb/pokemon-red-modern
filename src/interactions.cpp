#include "interactions.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <utility>

namespace pokered {
namespace {

bool read_u8(std::istream& input, std::uint8_t& result) {
    char value = 0;
    if (!input.get(value)) return false;
    result = static_cast<std::uint8_t>(static_cast<unsigned char>(value));
    return true;
}

bool read_u16(std::istream& input, std::uint16_t& result) {
    std::uint8_t low = 0;
    std::uint8_t high = 0;
    if (!read_u8(input, low) || !read_u8(input, high)) return false;
    result = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(low) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(high) << 8U));
    return true;
}

bool read_u32(std::istream& input, std::uint32_t& result) {
    std::array<std::uint8_t, 4> bytes{};
    for (std::uint8_t& byte : bytes)
        if (!read_u8(input, byte)) return false;
    result = static_cast<std::uint32_t>(bytes[0]) |
             static_cast<std::uint32_t>(bytes[1]) << 8U |
             static_cast<std::uint32_t>(bytes[2]) << 16U |
             static_cast<std::uint32_t>(bytes[3]) << 24U;
    return true;
}

bool read_owner(std::istream& input, InteractionOwner& owner) {
    return read_u8(input, owner.index) && read_u8(input, owner.x) &&
           read_u8(input, owner.y) && read_u8(input, owner.program_id);
}

bool read_owners(std::istream& input, std::vector<InteractionOwner>& owners) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count > 64U) return false;
    owners.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        InteractionOwner owner;
        if (!read_owner(input, owner) || owner.index != static_cast<std::uint8_t>(index + 1U))
            return false;
        owners.push_back(owner);
    }
    return true;
}

bool read_programs(std::istream& input, std::vector<InteractionProgram>& programs) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count > 256U) return false;
    programs.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        InteractionProgram program;
        std::uint8_t status = 0;
        std::uint16_t page_count = 0;
        if (!read_u8(input, status) || status > 3U || !read_u16(input, page_count) ||
            page_count > 64U)
            return false;
        program.status = static_cast<InteractionProgramStatus>(status);
        program.pages.reserve(page_count);
        for (std::uint16_t page = 0; page < page_count; ++page) {
            std::uint16_t size = 0;
            if (!read_u16(input, size) || size > 8192U) return false;
            std::string text(size, '\0');
            if (!input.read(text.data(), static_cast<std::streamsize>(text.size()))) return false;
            program.pages.push_back(std::move(text));
        }
        programs.push_back(std::move(program));
    }
    return true;
}

bool read_pages(std::istream& input, std::vector<std::string>& pages) {
    std::uint16_t page_count = 0U;
    if (!read_u16(input, page_count) || page_count > 64U)
        return false;
    pages.reserve(page_count);
    for (std::uint16_t page = 0U; page < page_count; ++page) {
        std::uint16_t size = 0U;
        if (!read_u16(input, size) || size == 0U || size > 8192U)
            return false;
        std::string text(size, '\0');
        if (!input.read(text.data(),
                        static_cast<std::streamsize>(text.size())))
            return false;
        pages.push_back(std::move(text));
    }
    return true;
}

bool read_trainers(std::istream& input,
                   std::vector<TrainerInteractionRule>& trainers) {
    std::uint16_t count = 0U;
    if (!read_u16(input, count) || count > 16U) return false;
    trainers.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        TrainerInteractionRule trainer;
        if (!read_u8(input, trainer.actor_index) ||
            trainer.actor_index == 0U ||
            !read_u8(input, trainer.sight_range) ||
            trainer.sight_range > 15U ||
            !read_u32(input, trainer.defeated_flag) ||
            !read_pages(input, trainer.before_pages) ||
            !read_pages(input, trainer.after_pages) ||
            !read_pages(input, trainer.end_pages))
            return false;
        trainers.push_back(std::move(trainer));
    }
    return true;
}

} // namespace

bool load_interactions(const std::filesystem::path& path, InteractionCatalog& result,
                       std::string& error) {
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    if (!input.read(magic.data(), static_cast<std::streamsize>(magic.size())) ||
        magic != std::array{'P', 'W', 'I', '2'}) {
        error = "world interaction cache is missing or has an invalid header";
        return false;
    }

    InteractionCatalog loaded;
    loaded.source = path;
    std::uint16_t map_count = 0;
    if (!read_u16(input, map_count) || map_count == 0U || map_count > 248U) {
        error = "world interaction cache has an invalid map count";
        return false;
    }
    loaded.maps.reserve(map_count);
    for (std::uint16_t index = 0; index < map_count; ++index) {
        MapInteractions map;
        std::uint8_t decoded = 0;
        if (!read_u8(input, map.map_id) || !read_u8(input, decoded) || decoded > 1U ||
            !read_owners(input, map.backgrounds) ||
            !read_owners(input, map.actors) ||
            !read_trainers(input, map.trainers) ||
            !read_programs(input, map.programs)) {
            error = "world interaction cache has an invalid map record";
            return false;
        }
        map.decoded = decoded != 0;
        loaded.maps.push_back(std::move(map));
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "world interaction cache contains trailing data";
        return false;
    }
    loaded.loaded = true;
    result = std::move(loaded);
    error.clear();
    return true;
}

const MapInteractions* find_map_interactions(const InteractionCatalog& catalog,
                                             std::uint8_t map_id) {
    const auto found =
        std::find_if(catalog.maps.begin(), catalog.maps.end(),
                     [map_id](const MapInteractions& map) { return map.map_id == map_id; });
    return found == catalog.maps.end() ? nullptr : &*found;
}

const InteractionProgram* find_interaction(const InteractionCatalog& catalog,
                                           std::uint8_t map_id, std::uint8_t program_id) {
    if (program_id == 0) return nullptr;
    const MapInteractions* map = find_map_interactions(catalog, map_id);
    const std::size_t index = static_cast<std::size_t>(program_id - 1U);
    if (map == nullptr || index >= map->programs.size()) return nullptr;
    return &map->programs[index];
}

const TrainerInteractionRule* find_trainer_interaction(
    const InteractionCatalog& catalog, std::uint8_t map_id,
    std::uint8_t actor_index) {
    const MapInteractions* map =
        find_map_interactions(catalog, map_id);
    if (map == nullptr) return nullptr;
    const auto found = std::ranges::find_if(
        map->trainers,
        [actor_index](const TrainerInteractionRule& trainer) {
            return trainer.actor_index == actor_index;
        });
    return found == map->trainers.end() ? nullptr : &*found;
}

} // namespace pokered
