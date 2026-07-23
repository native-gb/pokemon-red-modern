#include "import_pictures.hpp"

#include "gen1_picture_codec.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

constexpr std::size_t kBaseStatsOffset = 0x383DE;
constexpr std::size_t kBaseStatsSize = 28;
constexpr std::size_t kMewBaseStatsOffset = 0x0425B;
constexpr std::size_t kMonsterNamesOffset = 0x1C21E;
constexpr std::size_t kMonsterNameSize = 10;
constexpr std::size_t kInternalSpeciesCount = 190;
constexpr std::size_t kPokedexOrderOffset = 0x41024;
constexpr std::size_t kTrainerCount = 47;
constexpr std::size_t kTrainerPresentationOffset = 0x39914;
constexpr std::size_t kTrainerNamesOffset = 0x399FF;
constexpr std::size_t kTrainerNamesEnd = 0x39B87;
constexpr std::size_t kTrainerPictureBank = 0x13;

struct Picture {
    std::uint8_t width{};
    std::uint8_t height{};
    std::uint32_t rom_offset{};
    std::uint32_t compressed_size{};
    std::vector<std::uint8_t> pixels;
};

struct SpeciesPictures {
    std::string name;
    Picture front;
    Picture back;
};

struct TrainerPicture {
    std::string name;
    Picture picture;
};

bool has_range(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t size) {
    return offset <= bytes.size() && size <= bytes.size() - offset;
}

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

bool append_character(std::uint8_t value, std::string& text) {
    if (value >= 0x80 && value <= 0x99) {
        text.push_back(static_cast<char>('A' + value - 0x80));
        return true;
    }
    if (value >= 0xA0 && value <= 0xB9) {
        text.push_back(static_cast<char>('a' + value - 0xA0));
        return true;
    }
    if (value >= 0xF6) {
        text.push_back(static_cast<char>('0' + value - 0xF6));
        return true;
    }
    switch (value) {
    case 0x4A:
        text += "PKMN";
        return true;
    case 0x7F:
        text.push_back(' ');
        return true;
    case 0x9A:
        text.push_back('(');
        return true;
    case 0x9B:
        text.push_back(')');
        return true;
    case 0x9C:
        text.push_back(':');
        return true;
    case 0xBA:
        text += "é";
        return true;
    case 0xE0:
        text.push_back('\'');
        return true;
    case 0xE3:
        text.push_back('-');
        return true;
    case 0xE6:
        text.push_back('?');
        return true;
    case 0xE7:
        text.push_back('!');
        return true;
    case 0xE8:
        text.push_back('.');
        return true;
    case 0xEF:
        text += "♂";
        return true;
    case 0xF5:
        text += "♀";
        return true;
    default:
        return false;
    }
}

bool decode_fixed_name(std::span<const std::uint8_t> rom, std::size_t offset, std::string& name,
                       std::string& error) {
    name.clear();
    bool terminated = false;
    for (std::size_t index = 0; index < kMonsterNameSize; ++index) {
        const std::uint8_t value = rom[offset + index];
        if (value == 0x50) {
            terminated = true;
            continue;
        }
        if (terminated || !append_character(value, name)) {
            error = "MonsterNames contains malformed text";
            return false;
        }
    }
    return true;
}

bool decode_terminated_name(std::span<const std::uint8_t> rom, std::size_t& cursor,
                            std::string& name, std::string& error) {
    name.clear();
    while (cursor < kTrainerNamesEnd) {
        const std::uint8_t value = rom[cursor++];
        if (value == 0x50) return true;
        if (!append_character(value, name)) {
            error = "TrainerNames contains an unsupported character";
            return false;
        }
    }
    error = "TrainerNames is missing a terminator";
    return false;
}

std::string source_symbol(std::string_view prefix, std::string_view name, std::size_t ordinal) {
    std::ostringstream output;
    output << prefix << '_' << std::setfill('0') << std::setw(3) << ordinal << '_';
    bool separator = false;
    for (const char raw_character : name) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (std::isalnum(character) != 0) {
            output << static_cast<char>(std::tolower(character));
            separator = false;
        } else if (!separator) {
            output << '_';
            separator = true;
        }
    }
    std::string result = output.str();
    while (!result.empty() && result.back() == '_')
        result.pop_back();
    return result;
}

std::size_t picture_bank(std::uint8_t internal_id) {
    if (internal_id == 0x15) return 1;
    if (internal_id < 0x1F) return 9;
    if (internal_id < 0x4A) return 10;
    if (internal_id < 0x74) return 11;
    if (internal_id < 0x99) return 12;
    return 13;
}

bool pointer_offset(std::size_t bank, std::uint16_t pointer, std::size_t& result,
                    std::string& error) {
    if (pointer < 0x4000 || pointer >= 0x8000) {
        error = "picture pointer is outside $4000..$7fff";
        return false;
    }
    result = bank * 0x4000U + pointer - 0x4000U;
    return true;
}

std::vector<std::uint8_t> unpack_pixels(const DecodedGen1Picture& decoded) {
    const std::size_t width = static_cast<std::size_t>(decoded.width_tiles) * 8U;
    std::vector<std::uint8_t> pixels(width * width);
    for (std::size_t tile_y = 0; tile_y < decoded.height_tiles; ++tile_y) {
        for (std::size_t tile_x = 0; tile_x < decoded.width_tiles; ++tile_x) {
            const std::size_t tile =
                tile_y * static_cast<std::size_t>(decoded.width_tiles) + tile_x;
            for (std::size_t y = 0; y < 8; ++y) {
                const std::uint8_t low = decoded.two_bpp_bytes[tile * 16U + y * 2U];
                const std::uint8_t high = decoded.two_bpp_bytes[tile * 16U + y * 2U + 1U];
                for (std::size_t x = 0; x < 8; ++x) {
                    const unsigned bit = static_cast<unsigned>(7U - x);
                    pixels[(tile_y * 8U + y) * width + tile_x * 8U + x] =
                        static_cast<std::uint8_t>(((high >> bit) & 1U) << 1U | ((low >> bit) & 1U));
                }
            }
        }
    }
    return pixels;
}

bool decode_picture(std::span<const std::uint8_t> rom, std::size_t offset,
                    std::uint8_t expected_width, Picture& result, std::string& error) {
    if (offset >= rom.size()) {
        error = "picture offset extends outside the verified ROM";
        return false;
    }
    DecodedGen1Picture decoded;
    if (!decode_gen1_picture(rom.subspan(offset), decoded, error)) return false;
    if (decoded.width_tiles != expected_width || decoded.height_tiles != expected_width) {
        error = "picture dimensions disagree with their cartridge table";
        return false;
    }
    const std::size_t compressed_size = (decoded.bits_consumed + 7U) / 8U;
    if (compressed_size == 0 || compressed_size > 0xFFFFFFFFU || offset > 0xFFFFFFFFU) {
        error = "picture ROM provenance exceeds the cache format";
        return false;
    }
    result = {
        .width = decoded.width_tiles,
        .height = decoded.height_tiles,
        .rom_offset = static_cast<std::uint32_t>(offset),
        .compressed_size = static_cast<std::uint32_t>(compressed_size),
        .pixels = unpack_pixels(decoded),
    };
    return true;
}

void write_u16(std::vector<std::uint8_t>& output, std::size_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void write_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<std::uint8_t>(value >> shift));
}

void write_name(std::vector<std::uint8_t>& output, std::string_view name) {
    write_u16(output, name.size());
    output.insert(output.end(), name.begin(), name.end());
}

void write_picture(std::vector<std::uint8_t>& output, const Picture& picture) {
    output.push_back(picture.width);
    output.push_back(picture.height);
    write_u32(output, picture.rom_offset);
    write_u32(output, picture.compressed_size);
    write_u32(output, static_cast<std::uint32_t>(picture.pixels.size()));
    output.insert(output.end(), picture.pixels.begin(), picture.pixels.end());
}

void add_text_file(PictureImport& result, std::string path, const std::string& text) {
    result.files.push_back({std::move(path), std::vector<std::uint8_t>(text.begin(), text.end())});
}

} // namespace

bool decode_picture_import(std::span<const std::uint8_t> rom, PictureImport& result,
                           std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;
    if (!has_range(rom, kBaseStatsOffset, 150U * kBaseStatsSize) ||
        !has_range(rom, kMewBaseStatsOffset, kBaseStatsSize) ||
        !has_range(rom, kMonsterNamesOffset, kInternalSpeciesCount * kMonsterNameSize) ||
        !has_range(rom, kPokedexOrderOffset, kInternalSpeciesCount) ||
        !has_range(rom, kTrainerPresentationOffset, kTrainerCount * 5U) ||
        !has_range(rom, kTrainerNamesOffset, kTrainerNamesEnd - kTrainerNamesOffset)) {
        error = "picture import tables extend outside the verified ROM";
        return false;
    }

    std::array<std::uint8_t, 151> internal_ids{};
    std::array<std::string, 151> species_names;
    // Join internal picture-bank IDs to stable Pokédex order and display names.
    for (std::size_t index = 0; index < kInternalSpeciesCount; ++index) {
        const std::uint8_t dex = rom[kPokedexOrderOffset + index];
        if (dex == 0) continue;
        if (dex > internal_ids.size() || internal_ids[dex - 1U] != 0) {
            error = "PokedexOrder does not uniquely map all species";
            return false;
        }
        internal_ids[dex - 1U] = static_cast<std::uint8_t>(index + 1U);
        if (!decode_fixed_name(rom, kMonsterNamesOffset + index * kMonsterNameSize,
                               species_names[dex - 1U], error))
            return false;
    }

    std::vector<SpeciesPictures> species;
    species.reserve(151);
    std::ostringstream species_source;
    species_source << "; ROM-derived display bindings. Pixel data remains in compiled cache.\n";
    // Decode both battle views once and retain only normalized shade pixels at runtime.
    for (std::size_t dex_index = 0; dex_index < 151; ++dex_index) {
        if (internal_ids[dex_index] == 0 || species_names[dex_index].empty()) {
            error = "PokedexOrder does not resolve every species picture";
            return false;
        }
        const std::size_t stats =
            dex_index == 150 ? kMewBaseStatsOffset : kBaseStatsOffset + dex_index * kBaseStatsSize;
        if (rom[stats] != dex_index + 1U) {
            error = "BaseStats is not in Pokédex order";
            return false;
        }
        const std::uint8_t dimensions = rom[stats + 10U];
        const std::uint8_t width = static_cast<std::uint8_t>(dimensions >> 4U);
        const std::uint8_t height = static_cast<std::uint8_t>(dimensions & 0x0FU);
        if (width == 0 || width != height) {
            error = "BaseStats contains invalid front-picture dimensions";
            return false;
        }
        const std::size_t bank = picture_bank(internal_ids[dex_index]);
        std::size_t front_offset = 0;
        std::size_t back_offset = 0;
        if (!pointer_offset(bank, read_u16(rom, stats + 11U), front_offset, error) ||
            !pointer_offset(bank, read_u16(rom, stats + 13U), back_offset, error))
            return false;

        SpeciesPictures pictures;
        pictures.name = species_names[dex_index];
        if (!decode_picture(rom, front_offset, width, pictures.front, error) ||
            !decode_picture(rom, back_offset, 4, pictures.back, error)) {
            error = pictures.name + ": " + error;
            return false;
        }
        const std::string symbol = source_symbol("pokemon", pictures.name, dex_index + 1U);
        species_source << "pokemon_visual " << symbol << '\n'
                       << "    display_name \"" << pictures.name << "\"\n"
                       << "    front " << static_cast<unsigned>(pictures.front.width) << ' '
                       << static_cast<unsigned>(pictures.front.height) << " \"0x" << std::hex
                       << std::uppercase << pictures.front.rom_offset << std::dec << "\"\n"
                       << "    back " << static_cast<unsigned>(pictures.back.width) << ' '
                       << static_cast<unsigned>(pictures.back.height) << " \"0x" << std::hex
                       << std::uppercase << pictures.back.rom_offset << std::dec << "\"\n\n";
        species.push_back(std::move(pictures));
    }

    std::vector<TrainerPicture> trainers;
    trainers.reserve(kTrainerCount);
    std::map<std::size_t, Picture> decoded_trainers;
    std::size_t name_cursor = kTrainerNamesOffset;
    std::ostringstream trainer_source;
    trainer_source << "; Trainer-class bindings may share one ROM portrait.\n";
    // Preserve all class bindings while decoding shared portrait offsets once.
    for (std::size_t index = 0; index < kTrainerCount; ++index) {
        TrainerPicture trainer;
        if (!decode_terminated_name(rom, name_cursor, trainer.name, error)) return false;
        std::size_t picture_offset = 0;
        if (!pointer_offset(kTrainerPictureBank,
                            read_u16(rom, kTrainerPresentationOffset + index * 5U), picture_offset,
                            error))
            return false;
        const auto found = decoded_trainers.find(picture_offset);
        if (found == decoded_trainers.end()) {
            Picture picture;
            if (!decode_picture(rom, picture_offset, 7, picture, error)) {
                error = trainer.name + ": " + error;
                return false;
            }
            trainer.picture = picture;
            decoded_trainers.emplace(picture_offset, std::move(picture));
        } else {
            trainer.picture = found->second;
        }
        trainer_source << "trainer_visual " << source_symbol("trainer", trainer.name, index + 1U)
                       << '\n'
                       << "    display_name \"" << trainer.name << "\"\n"
                       << "    portrait 7 7 \"0x" << std::hex << std::uppercase
                       << trainer.picture.rom_offset << std::dec << "\"\n\n";
        trainers.push_back(std::move(trainer));
    }
    if (name_cursor != kTrainerNamesEnd) {
        error = "TrainerNames did not consume its exact ROM range";
        return false;
    }

    // Write one compact, host-independent cache for direct upload by a renderer.
    std::vector<std::uint8_t> cache{'P', 'G', 'P', '1'};
    write_u16(cache, species.size());
    write_u16(cache, trainers.size());
    for (const SpeciesPictures& pictures : species) {
        write_name(cache, pictures.name);
        write_picture(cache, pictures.front);
        write_picture(cache, pictures.back);
    }
    for (const TrainerPicture& trainer : trainers) {
        write_name(cache, trainer.name);
        write_picture(cache, trainer.picture);
    }
    result.files.push_back({"compiled/battle_pictures.bin", std::move(cache)});
    add_text_file(result, "source/graphics/pokemon_visuals.sexpr", species_source.str());
    add_text_file(result, "source/graphics/trainer_visuals.sexpr", trainer_source.str());

    std::ostringstream report;
    report << "Pokemon Red US Rev 0 battle-picture import\n"
           << "species " << species.size() << '\n'
           << "front_pictures " << species.size() << '\n'
           << "back_pictures " << species.size() << '\n'
           << "trainer_classes " << trainers.size() << '\n'
           << "unique_trainer_portraits " << decoded_trainers.size() << '\n'
           << "runtime_cache compiled/battle_pictures.bin\n";
    add_text_file(result, "reports/picture_import_summary.txt", report.str());

    result.species = species.size();
    result.front_pictures = species.size();
    result.back_pictures = species.size();
    result.trainer_classes = trainers.size();
    error.clear();
    return true;
}

} // namespace pokered::import
