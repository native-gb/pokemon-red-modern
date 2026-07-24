#include "import_boot.hpp"

#include "boot.hpp"
#include "gen1_picture_codec.hpp"
#include "import_text.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

constexpr std::size_t kTitleRoutineOffset = 0x042B7;
constexpr std::size_t kTitleRoutineEnd = 0x045B6;
constexpr std::size_t kTitleCopyrightTilesOffset = 0x0437F;
constexpr std::size_t kTitleBounceOffset = 0x043DB;
constexpr std::size_t kTitlePokemonOffset = 0x04588;
constexpr std::size_t kTitleVersionTilesOffset = 0x045A1;
constexpr std::size_t kTitleScrollOffset = 0x37244;
constexpr std::size_t kTitleScrollEnd = 0x372D6;
constexpr std::size_t kTitleScrollInOffset = 0x37247;
constexpr std::size_t kTitleScrollOutOffset = 0x3724F;
constexpr std::size_t kTitleBallYPositionsOffset = 0x372A0;

constexpr std::size_t kPokemonLogoOffset = 0x11380;
constexpr std::size_t kCopyrightGraphicsOffset = 0x120C8;
constexpr std::size_t kGameFreakGraphicsOffset = 0x121F8;
constexpr std::size_t kTitlePlayerGraphicsOffset = 0x126A8;
constexpr std::size_t kVersionGraphicsOffset = 0x6802F;
constexpr std::size_t kFontGraphicsOffset = 0x11A80;
constexpr std::size_t kTextBoxGraphicsOffset = 0x12288;
constexpr std::size_t kHpStatusGraphicsOffset = 0x11EA0;
constexpr std::size_t kPokedexGraphicsOffset = 0x12488;
constexpr std::size_t kPokedexTileCount = 18U;

constexpr std::size_t kBaseStatsOffset = 0x383DE;
constexpr std::size_t kBaseStatsSize = 28;
constexpr std::size_t kMewBaseStatsOffset = 0x0425B;
constexpr std::size_t kPokedexOrderOffset = 0x41024;
constexpr std::size_t kInternalSpeciesCount = 190;

constexpr std::size_t kProfessorPictureOffset = 0x4E15F;
constexpr std::size_t kNidorinoPictureOffset = 0x35282;
constexpr std::size_t kPlayerPictureOffset = 0x12EDE;
constexpr std::size_t kRivalPictureOffset = 0x4E049;
constexpr std::size_t kShrinkPicture1Offset = 0x12FE8;
constexpr std::size_t kShrinkPicture2Offset = 0x13042;

constexpr std::size_t kPrepareOakSpeechOffset = 0x060CA;
constexpr std::size_t kOakSpeechOffset = 0x06115;
constexpr std::size_t kOakSpeechEnd = 0x06253;
constexpr std::size_t kIntroFadePalettesOffset = 0x06282;
constexpr std::size_t kNameRoutinesOffset = 0x0695D;
constexpr std::size_t kNameRoutinesEnd = 0x06B20;
constexpr std::size_t kPlayerNamesOffset = 0x06AA8;
constexpr std::size_t kPlayerNamesEnd = 0x06ABE;
constexpr std::size_t kRivalNamesOffset = 0x06ABE;
constexpr std::size_t kRivalNamesEnd = 0x06AD6;

constexpr std::array<std::pair<std::string_view, std::size_t>, 8> kOakTextProfiles{{
    {"oak_greeting", 0x8A425},
    {"oak_creature_introduction", 0x8A47F},
    {"oak_creature_explanation", 0x8A4B3},
    {"oak_player_introduction", 0x8A519},
    {"oak_player_confirmation", 0x8A62F},
    {"oak_rival_introduction", 0x8A534},
    {"oak_rival_confirmation", 0x8A64A},
    {"oak_final_speech", 0x8A597},
}};

constexpr std::array<std::tuple<std::size_t, std::size_t, std::string_view>, 3>
    kOptionTextProfiles{{
        {0x05FC0, 0x05FDE, "text_speed"},
        {0x05FDE, 0x05FFD, "battle_animation"},
        {0x05FFD, 0x06018, "battle_style"},
    }};

bool has_range(std::span<const std::uint8_t> rom, std::size_t offset, std::size_t size) {
    return offset <= rom.size() && size <= rom.size() - offset;
}

std::uint16_t read_u16(std::span<const std::uint8_t> rom, std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(rom[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(rom[offset + 1U]) << 8U));
}

void write_u16(std::vector<std::uint8_t>& output, std::size_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void write_u32(std::vector<std::uint8_t>& output, std::size_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8U)
        output.push_back(static_cast<std::uint8_t>(value >> shift));
}

bool write_string(std::vector<std::uint8_t>& output, std::string_view text,
                  std::string& error) {
    if (text.empty() || text.size() > std::numeric_limits<std::uint16_t>::max()) {
        error = "boot import attempted to emit an invalid string";
        return false;
    }
    write_u16(output, text.size());
    output.insert(output.end(), text.begin(), text.end());
    return true;
}

void add_file(BootImport& result, std::string path, std::string_view text) {
    result.files.push_back(
        {std::move(path), std::vector<std::uint8_t>(text.begin(), text.end())});
}

std::string glyph(std::uint8_t value) {
    if (value >= 0x80U && value <= 0x99U)
        return std::string(1, static_cast<char>('A' + value - 0x80U));
    if (value >= 0xA0U && value <= 0xB9U)
        return std::string(1, static_cast<char>('a' + value - 0xA0U));
    if (value >= 0xF6U) return std::string(1, static_cast<char>('0' + value - 0xF6U));
    switch (value) {
    case 0x4A:
        return "PKMN";
    case 0x54:
        return "POKé";
    case 0x7F:
        return " ";
    case 0x9A:
        return "(";
    case 0x9B:
        return ")";
    case 0x9C:
        return ":";
    case 0x9D:
        return ";";
    case 0x9E:
        return "[";
    case 0x9F:
        return "]";
    case 0xBA:
        return "é";
    case 0xE0:
        return "'";
    case 0xE1:
        return "PK";
    case 0xE2:
        return "MN";
    case 0xE3:
        return "-";
    case 0xE6:
        return "?";
    case 0xE7:
        return "!";
    case 0xE8:
        return ".";
    case 0xEF:
        return "♂";
    case 0xF1:
        return "×";
    case 0xF3:
        return "/";
    case 0xF4:
        return ",";
    case 0xF5:
        return "♀";
    default:
        break;
    }
    std::ostringstream token;
    token << "{glyph_" << std::hex << std::setfill('0') << std::setw(2)
          << static_cast<unsigned>(value) << '}';
    return token.str();
}

bool decode_fixed_text(std::span<const std::uint8_t> rom, std::size_t begin,
                       std::size_t end, std::string& result, std::string& error) {
    if (begin >= end || !has_range(rom, begin, end - begin)) {
        error = "boot text extends outside the verified ROM";
        return false;
    }
    result.clear();
    for (std::size_t cursor = begin; cursor < end; ++cursor) {
        const std::uint8_t value = rom[cursor];
        if (value == 0x50U) break;
        if (value == 0x4EU || value == 0x4FU || value == 0x55U) {
            if (!result.empty() && result.back() != ' ') result.push_back(' ');
            continue;
        }
        result += glyph(value);
    }
    while (!result.empty() && result.back() == ' ')
        result.pop_back();
    if (result.empty()) {
        error = "boot text decoded to an empty label";
        return false;
    }
    return true;
}

bool decode_name_choices(std::span<const std::uint8_t> rom, std::size_t begin,
                         std::size_t end, std::array<std::string, 4>& choices,
                         std::string& error) {
    if (!has_range(rom, begin, end - begin)) {
        error = "boot name choices extend outside the verified ROM";
        return false;
    }
    std::size_t cursor = begin;
    for (std::size_t index = 0; index < choices.size(); ++index) {
        const std::uint8_t terminator = index + 1U == choices.size() ? 0x50U : 0x4EU;
        const auto found = std::find(rom.begin() + static_cast<std::ptrdiff_t>(cursor),
                                     rom.begin() + static_cast<std::ptrdiff_t>(end), terminator);
        if (found == rom.begin() + static_cast<std::ptrdiff_t>(end)) {
            error = "boot name choices have a missing separator";
            return false;
        }
        const std::size_t separator = static_cast<std::size_t>(found - rom.begin());
        if (!decode_fixed_text(rom, cursor, separator, choices[index], error)) return false;
        cursor = separator + 1U;
    }
    if (cursor != end) {
        error = "boot name choices do not consume their exact source span";
        return false;
    }
    return true;
}

void decode_one_bpp_tile(std::span<const std::uint8_t> rom, std::size_t offset,
                         std::array<std::uint8_t, 64>& pixels) {
    for (std::size_t y = 0; y < 8U; ++y) {
        const std::uint8_t row = rom[offset + y];
        for (std::size_t x = 0; x < 8U; ++x) {
            const unsigned bit = static_cast<unsigned>(7U - x);
            pixels[y * 8U + x] = ((row >> bit) & 1U) == 0U ? 0U : 3U;
        }
    }
}

void decode_two_bpp_tile(std::span<const std::uint8_t> rom, std::size_t offset,
                         std::array<std::uint8_t, 64>& pixels) {
    for (std::size_t y = 0; y < 8U; ++y) {
        const std::uint8_t low = rom[offset + y * 2U];
        const std::uint8_t high = rom[offset + y * 2U + 1U];
        for (std::size_t x = 0; x < 8U; ++x) {
            const unsigned bit = static_cast<unsigned>(7U - x);
            pixels[y * 8U + x] = static_cast<std::uint8_t>(
                ((high >> bit) & 1U) << 1U | ((low >> bit) & 1U));
        }
    }
}

bool raw_image(std::span<const std::uint8_t> rom, std::string key, std::size_t offset,
               std::size_t tile_count, std::size_t columns, bool one_bpp,
               bool transparent, BootImage& result, std::string& error) {
    const std::size_t bytes_per_tile = one_bpp ? 8U : 16U;
    if (tile_count == 0U || columns == 0U || tile_count % columns != 0U ||
        !has_range(rom, offset, tile_count * bytes_per_tile)) {
        error = "boot raw graphic has an invalid verified span";
        return false;
    }
    const std::size_t rows = tile_count / columns;
    const std::size_t width = columns * 8U;
    const std::size_t height = rows * 8U;
    if (width > std::numeric_limits<std::uint16_t>::max() ||
        height > std::numeric_limits<std::uint16_t>::max()) {
        error = "boot raw graphic exceeds the cache dimensions";
        return false;
    }
    BootImage image{
        .key = std::move(key),
        .width = static_cast<std::uint16_t>(width),
        .height = static_cast<std::uint16_t>(height),
        .transparent = transparent,
        .pixels = std::vector<std::uint8_t>(width * height),
    };
    for (std::size_t tile = 0; tile < tile_count; ++tile) {
        std::array<std::uint8_t, 64> decoded{};
        if (one_bpp)
            decode_one_bpp_tile(rom, offset + tile * bytes_per_tile, decoded);
        else
            decode_two_bpp_tile(rom, offset + tile * bytes_per_tile, decoded);
        const std::size_t tile_x = tile % columns;
        const std::size_t tile_y = tile / columns;
        for (std::size_t y = 0; y < 8U; ++y)
            std::copy_n(decoded.begin() + static_cast<std::ptrdiff_t>(y * 8U), 8U,
                        image.pixels.begin() +
                            static_cast<std::ptrdiff_t>(
                                (tile_y * 8U + y) * width + tile_x * 8U));
    }
    result = std::move(image);
    return true;
}

bool compressed_image(std::span<const std::uint8_t> rom, std::string key,
                      std::size_t offset, std::uint8_t expected_width,
                      BootImage& result, std::string& error) {
    if (offset >= rom.size()) {
        error = "boot compressed picture starts outside the verified ROM";
        return false;
    }
    DecodedGen1Picture decoded;
    if (!decode_gen1_picture(rom.subspan(offset), decoded, error)) return false;
    if (decoded.width_tiles != expected_width || decoded.height_tiles != expected_width) {
        error = "boot compressed picture dimensions disagree with its verified profile";
        return false;
    }
    const std::size_t width = static_cast<std::size_t>(expected_width) * 8U;
    BootImage image{
        .key = std::move(key),
        .width = static_cast<std::uint16_t>(width),
        .height = static_cast<std::uint16_t>(width),
        .transparent = true,
        .pixels = std::vector<std::uint8_t>(width * width),
    };
    for (std::size_t tile_y = 0; tile_y < expected_width; ++tile_y) {
        for (std::size_t tile_x = 0; tile_x < expected_width; ++tile_x) {
            const std::size_t tile =
                tile_y * static_cast<std::size_t>(expected_width) + tile_x;
            for (std::size_t y = 0; y < 8U; ++y) {
                const std::uint8_t low = decoded.two_bpp_bytes[tile * 16U + y * 2U];
                const std::uint8_t high = decoded.two_bpp_bytes[tile * 16U + y * 2U + 1U];
                for (std::size_t x = 0; x < 8U; ++x) {
                    const unsigned bit = static_cast<unsigned>(7U - x);
                    image.pixels[(tile_y * 8U + y) * width + tile_x * 8U + x] =
                        static_cast<std::uint8_t>(
                            ((high >> bit) & 1U) << 1U | ((low >> bit) & 1U));
                }
            }
        }
    }
    result = std::move(image);
    return true;
}

std::size_t picture_bank(std::uint8_t internal_id) {
    if (internal_id == 0x15U) return 1U;
    if (internal_id < 0x1FU) return 9U;
    if (internal_id < 0x4AU) return 10U;
    if (internal_id < 0x74U) return 11U;
    if (internal_id < 0x99U) return 12U;
    return 13U;
}

bool species_front_image(std::span<const std::uint8_t> rom, std::uint8_t internal_id,
                         std::string key, BootImage& result, std::string& error) {
    if (internal_id == 0U || internal_id > kInternalSpeciesCount ||
        !has_range(rom, kPokedexOrderOffset + internal_id - 1U, 1U)) {
        error = "title species has an invalid internal identity";
        return false;
    }
    const std::uint8_t dex = rom[kPokedexOrderOffset + internal_id - 1U];
    if (dex == 0U || dex > 151U) {
        error = "title species resolves to an unused internal slot";
        return false;
    }
    const std::size_t stats =
        dex == 151U ? kMewBaseStatsOffset
                    : kBaseStatsOffset + static_cast<std::size_t>(dex - 1U) * kBaseStatsSize;
    if (!has_range(rom, stats, kBaseStatsSize) || rom[stats] != dex) {
        error = "title species has no matching base-stat record";
        return false;
    }
    const std::uint8_t dimensions = rom[stats + 10U];
    const std::uint8_t width = static_cast<std::uint8_t>(dimensions >> 4U);
    if (width == 0U || width != static_cast<std::uint8_t>(dimensions & 0x0FU)) {
        error = "title species has invalid front-picture dimensions";
        return false;
    }
    const std::uint16_t pointer = read_u16(rom, stats + 11U);
    if (pointer < 0x4000U || pointer >= 0x8000U) {
        error = "title species has an invalid front-picture pointer";
        return false;
    }
    const std::size_t offset =
        picture_bank(internal_id) * 0x4000U + static_cast<std::size_t>(pointer - 0x4000U);
    return compressed_image(rom, std::move(key), offset, width, result, error);
}

std::uint16_t push_image(std::vector<BootImage>& images, BootImage image,
                         std::string& error) {
    if (images.size() >= std::numeric_limits<std::uint16_t>::max()) {
        error = "boot import has too many images";
        return std::numeric_limits<std::uint16_t>::max();
    }
    const auto id = static_cast<std::uint16_t>(images.size());
    images.push_back(std::move(image));
    return id;
}

bool build_ui_tiles(std::span<const std::uint8_t> rom,
                    std::vector<std::uint8_t>& pixels, std::string& error) {
    constexpr std::size_t tile_count = 256U;
    if (!has_range(rom, kFontGraphicsOffset, 128U * 8U) ||
        !has_range(rom, kTextBoxGraphicsOffset, 32U * 16U) ||
        !has_range(rom, kHpStatusGraphicsOffset, 30U * 16U)) {
        error = "boot UI graphics extend outside the verified ROM";
        return false;
    }
    pixels.assign(tile_count * 64U, 0U);
    const auto copy_tiles = [&](std::size_t offset, std::size_t count,
                                std::size_t first, bool one_bpp) {
        for (std::size_t tile = 0; tile < count; ++tile) {
            std::array<std::uint8_t, 64> decoded{};
            if (one_bpp)
                decode_one_bpp_tile(rom, offset + tile * 8U, decoded);
            else
                decode_two_bpp_tile(rom, offset + tile * 16U, decoded);
            std::copy(decoded.begin(), decoded.end(),
                      pixels.begin() + static_cast<std::ptrdiff_t>((first + tile) * 64U));
        }
    };
    copy_tiles(kFontGraphicsOffset, 128U, 0x80U, true);
    copy_tiles(kTextBoxGraphicsOffset, 32U, 0x60U, false);
    copy_tiles(kHpStatusGraphicsOffset, 30U, 0x62U, false);
    return true;
}

bool build_pokedex_tiles(
    std::span<const std::uint8_t> rom,
    std::vector<std::uint8_t>& pixels,
    std::string& error) {
    if (!has_range(
            rom, kPokedexGraphicsOffset,
            kPokedexTileCount * 16U)) {
        error =
            "Pokedex UI graphics extend outside the verified ROM";
        return false;
    }
    pixels.resize(kPokedexTileCount * 64U);
    for (std::size_t tile = 0U;
         tile < kPokedexTileCount; ++tile) {
        std::array<std::uint8_t, 64> decoded{};
        decode_two_bpp_tile(
            rom, kPokedexGraphicsOffset + tile * 16U,
            decoded);
        std::copy(
            decoded.begin(), decoded.end(),
            pixels.begin() +
                static_cast<std::ptrdiff_t>(tile * 64U));
    }
    return true;
}

bool build_title(std::span<const std::uint8_t> rom, std::vector<BootImage>& images,
                 BootTitleDefinition& title, std::ostringstream& source,
                 std::string& error) {
    if (!has_range(rom, kTitleRoutineOffset, kTitleRoutineEnd - kTitleRoutineOffset) ||
        !has_range(rom, kTitleScrollOffset, kTitleScrollEnd - kTitleScrollOffset)) {
        error = "title routines extend outside the verified ROM";
        return false;
    }

    BootImage image;
    if (!raw_image(rom, "pokemon_logo", kPokemonLogoOffset, 112U, 16U, false,
                   true, image, error))
        return false;
    title.logo_image = push_image(images, std::move(image), error);

    // Copyright and Game Freak use one cartridge tile-ID strip. Precompose it
    // during import so runtime rendering has no Game Boy VRAM vocabulary.
    BootImage copyright_tiles;
    BootImage game_freak_tiles;
    if (!raw_image(rom, "copyright_tiles", kCopyrightGraphicsOffset, 19U, 19U,
                   false, true, copyright_tiles, error) ||
        !raw_image(rom, "game_freak_tiles", kGameFreakGraphicsOffset, 9U, 9U,
                   false, true, game_freak_tiles, error))
        return false;
    BootImage copyright_strip{
        .key = "copyright_strip",
        .width = 128,
        .height = 8,
        .transparent = true,
        .pixels = std::vector<std::uint8_t>(128U * 8U),
    };
    for (std::size_t index = 0; index < 16U; ++index) {
        const std::uint8_t tile = rom[kTitleCopyrightTilesOffset + index];
        const BootImage* owner = nullptr;
        std::size_t source_tile = 0;
        if (tile >= 0x41U && tile <= 0x45U) {
            owner = &copyright_tiles;
            source_tile = tile - 0x41U;
        } else if (tile >= 0x46U && tile <= 0x4EU) {
            owner = &game_freak_tiles;
            source_tile = tile - 0x46U;
        } else {
            error = "title copyright strip references an invalid graphic tile";
            return false;
        }
        for (std::size_t y = 0; y < 8U; ++y)
            std::copy_n(owner->pixels.begin() +
                            static_cast<std::ptrdiff_t>(
                                y * owner->width + source_tile * 8U),
                        8U,
                        copyright_strip.pixels.begin() +
                            static_cast<std::ptrdiff_t>(y * 128U + index * 8U));
    }
    title.copyright_image = push_image(images, std::move(copyright_strip), error);

    BootImage version_tiles;
    if (!raw_image(rom, "version_tiles", kVersionGraphicsOffset, 10U, 10U, true,
                   true, version_tiles, error))
        return false;
    BootImage version_strip{
        .key = "version_strip",
        .width = 64,
        .height = 8,
        .transparent = true,
        .pixels = std::vector<std::uint8_t>(64U * 8U),
    };
    for (std::size_t index = 0; index < 8U; ++index) {
        const std::uint8_t tile = rom[kTitleVersionTilesOffset + index];
        if (tile == 0x7FU) continue;
        if (tile < 0x60U || tile > 0x69U) {
            error = "title version strip references an invalid graphic tile";
            return false;
        }
        const std::size_t source_tile = tile - 0x60U;
        for (std::size_t y = 0; y < 8U; ++y)
            std::copy_n(version_tiles.pixels.begin() +
                            static_cast<std::ptrdiff_t>(
                                y * version_tiles.width + source_tile * 8U),
                        8U,
                        version_strip.pixels.begin() +
                            static_cast<std::ptrdiff_t>(y * 64U + index * 8U));
    }
    title.version_image = push_image(images, std::move(version_strip), error);

    if (!raw_image(rom, "title_player", kTitlePlayerGraphicsOffset, 35U, 5U,
                   false, true, image, error))
        return false;
    BootImage ball{
        .key = "title_ball",
        .width = 8,
        .height = 8,
        .transparent = true,
        .pixels = std::vector<std::uint8_t>(64U),
    };
    for (std::size_t y = 0; y < 8U; ++y) {
        std::copy_n(image.pixels.begin() +
                        static_cast<std::ptrdiff_t>((2U * 8U + y) * 40U),
                    8U,
                    ball.pixels.begin() + static_cast<std::ptrdiff_t>(y * 8U));
        std::fill_n(image.pixels.begin() +
                        static_cast<std::ptrdiff_t>((2U * 8U + y) * 40U),
                    8U, 0U);
    }
    title.player_image = push_image(images, std::move(image), error);
    title.ball_image = push_image(images, std::move(ball), error);

    title.species.reserve(16U);
    for (std::size_t index = 0; index < 16U; ++index) {
        const std::uint8_t internal_id = rom[kTitlePokemonOffset + index];
        std::ostringstream key;
        key << "title_species_" << std::setfill('0') << std::setw(3)
            << static_cast<unsigned>(internal_id);
        if (!species_front_image(rom, internal_id, key.str(), image, error)) return false;
        const std::uint16_t image_id = push_image(images, std::move(image), error);
        title.species.push_back({internal_id, image_id});
    }

    std::size_t bounce_cursor = kTitleBounceOffset;
    for (std::size_t index = 0; index < 7U; ++index) {
        const auto speed = static_cast<std::int8_t>(rom[bounce_cursor++]);
        const std::uint8_t frames = rom[bounce_cursor++];
        if (speed == 0 || frames == 0U) {
            error = "title logo bounce contains an invalid segment";
            return false;
        }
        title.logo_bounce.push_back({speed, frames});
    }
    if (rom[bounce_cursor] != 0U) {
        error = "title logo bounce has no exact terminator";
        return false;
    }
    title.pokemon_scroll_in.assign(
        rom.begin() + static_cast<std::ptrdiff_t>(kTitleScrollInOffset),
        rom.begin() + static_cast<std::ptrdiff_t>(kTitleScrollInOffset + 7U));
    title.pokemon_scroll_out.assign(
        rom.begin() + static_cast<std::ptrdiff_t>(kTitleScrollOutOffset),
        rom.begin() + static_cast<std::ptrdiff_t>(kTitleScrollOutOffset + 8U));
    title.ball_y_positions.assign(
        rom.begin() + static_cast<std::ptrdiff_t>(kTitleBallYPositionsOffset),
        rom.begin() + static_cast<std::ptrdiff_t>(kTitleBallYPositionsOffset + 11U));
    title.setup_frames = 58U;
    title.after_logo_delay_frames = 36U;
    title.version_scroll_frames = 28U;
    title.interruption_wait_frames = 200U;

    source << "title_sequence pokemon_red_title\n"
           << "    source \"0x042b7\" \"0x045b6\"\n"
           << "    setup_frames " << title.setup_frames << '\n'
           << "    after_logo_delay " << title.after_logo_delay_frames << '\n'
           << "    version_scroll_frames " << title.version_scroll_frames << '\n'
           << "    interruption_wait " << title.interruption_wait_frames << '\n';
    for (const BootTitleBounce& bounce : title.logo_bounce)
        source << "    logo_bounce " << static_cast<int>(bounce.pixels_per_frame) << ' '
               << static_cast<unsigned>(bounce.frame_count) << '\n';
    for (const BootTitleSpecies& species : title.species)
        source << "    title_species internal_" << static_cast<unsigned>(species.internal_id)
               << '\n';
    source << '\n';
    return error.empty();
}

bool build_menu(std::span<const std::uint8_t> rom, BootMenuDefinition& menu,
                std::ostringstream& source, std::string& error) {
    if (!decode_fixed_text(rom, 0x05D7E, 0x05D86, menu.continue_label, error) ||
        !decode_fixed_text(rom, 0x05D87, 0x05D8F, menu.new_game_label, error) ||
        !decode_fixed_text(rom, 0x05D90, 0x05D96, menu.option_label, error))
        return false;
    for (std::size_t index = 0; index < kOptionTextProfiles.size(); ++index) {
        const auto [begin, end, key] = kOptionTextProfiles[index];
        if (!decode_fixed_text(rom, begin, end, menu.option_rows[index], error)) return false;
        source << "option_row " << key << "\n    text \"" << menu.option_rows[index]
               << "\"\n";
    }
    if (!decode_fixed_text(rom, 0x06018, 0x0601F, menu.option_cancel, error)) return false;
    menu.before_input_delay_frames = 20U;
    source << "\nmain_menu pokemon_red_main_menu\n"
           << "    entry continue \"" << menu.continue_label << "\"\n"
           << "    entry new_game \"" << menu.new_game_label << "\"\n"
           << "    entry option \"" << menu.option_label << "\"\n"
           << "    before_input_delay 20\n\n";
    return true;
}

bool build_oak(std::span<const std::uint8_t> rom, std::vector<BootImage>& images,
               BootOakDefinition& oak, std::ostringstream& source,
               std::string& error) {
    if (!has_range(rom, kPrepareOakSpeechOffset,
                   kOakSpeechEnd - kPrepareOakSpeechOffset) ||
        !has_range(rom, kNameRoutinesOffset, kNameRoutinesEnd - kNameRoutinesOffset) ||
        !decode_name_choices(rom, kPlayerNamesOffset, kPlayerNamesEnd,
                             oak.player_names, error) ||
        !decode_name_choices(rom, kRivalNamesOffset, kRivalNamesEnd,
                             oak.rival_names, error))
        return false;

    constexpr std::array<std::tuple<std::string_view, std::size_t, std::uint8_t>, 6>
        profiles{{
            {"professor_oak", kProfessorPictureOffset, 7},
            {"nidorino", kNidorinoPictureOffset, 6},
            {"player", kPlayerPictureOffset, 7},
            {"rival", kRivalPictureOffset, 7},
            {"player_shrink_1", kShrinkPicture1Offset, 7},
            {"player_shrink_2", kShrinkPicture2Offset, 7},
        }};
    BootImage image;
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        const auto [key, offset, width] = profiles[index];
        if (!compressed_image(rom, std::string(key), offset, width, image, error)) return false;
        oak.picture_images[index] = push_image(images, std::move(image), error);
    }

    for (std::size_t index = 0; index < kOakTextProfiles.size(); ++index) {
        const auto [key, offset] = kOakTextProfiles[index];
        DecodedTextProgram decoded;
        if (!decode_text_program(rom, 0x22U, offset, decoded) || !decoded.complete ||
            decoded.dynamic || decoded.pages.empty()) {
            error = "Oak introduction text did not decode to a complete static program: ";
            error += key;
            if (!decoded.unresolved_reason.empty()) {
                error += " (";
                error += decoded.unresolved_reason;
                error += ')';
            }
            return false;
        }
        oak.texts[index] = {
            .key = std::string(key),
            .pages = std::move(decoded.pages),
        };
        source << "text_program " << key << '\n' << decoded.operations << '\n';
    }
    std::copy_n(rom.begin() + static_cast<std::ptrdiff_t>(kIntroFadePalettesOffset),
                oak.fade_palettes.size(), oak.fade_palettes.begin());
    oak.fade_step_delay_frames = 10U;
    oak.slide_step_delay_frames = 3U;
    oak.slide_steps = 6U;
    oak.ending_delay_frames = {4U, 4U, 20U, 50U};

    source << "oak_sequence pokemon_red_oak\n"
           << "    source \"0x060ca\" \"0x06253\"\n"
           << "    player_names";
    for (const std::string& name : oak.player_names) source << " \"" << name << '"';
    source << "\n    rival_names";
    for (const std::string& name : oak.rival_names) source << " \"" << name << '"';
    source << "\n    slide_step_delay " << static_cast<unsigned>(oak.slide_step_delay_frames)
           << "\n    slide_steps " << static_cast<unsigned>(oak.slide_steps) << "\n\n";
    return true;
}

bool write_image(std::vector<std::uint8_t>& cache, const BootImage& image,
                 std::string& error) {
    if (!write_string(cache, image.key, error) || image.width == 0U || image.height == 0U ||
        image.pixels.size() !=
            static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height)) {
        if (error.empty()) error = "boot image has invalid dimensions";
        return false;
    }
    write_u16(cache, image.width);
    write_u16(cache, image.height);
    cache.push_back(image.transparent ? 1U : 0U);
    write_u32(cache, image.pixels.size());
    cache.insert(cache.end(), image.pixels.begin(), image.pixels.end());
    return true;
}

bool write_text(std::vector<std::uint8_t>& cache, const BootTextProgram& text,
                std::string& error) {
    if (!write_string(cache, text.key, error) || text.pages.empty() ||
        text.pages.size() > std::numeric_limits<std::uint16_t>::max()) {
        if (error.empty()) error = "boot text program has no pages";
        return false;
    }
    write_u16(cache, text.pages.size());
    for (const std::string& page : text.pages)
        if (!write_string(cache, page, error)) return false;
    return true;
}

bool emit_cache(const std::vector<BootImage>& images,
                const std::vector<std::uint8_t>& ui_tiles,
                const std::vector<std::uint8_t>& pokedex_tiles,
                const BootTitleDefinition& title, const BootMenuDefinition& menu,
                const BootOakDefinition& oak, BootImport& result,
                std::string& error) {
    std::vector<std::uint8_t> cache{'P', 'B', 'T', '3'};
    write_u16(cache, images.size());
    for (const BootImage& image : images)
        if (!write_image(cache, image, error)) return false;
    write_u32(cache, ui_tiles.size());
    cache.insert(cache.end(), ui_tiles.begin(), ui_tiles.end());
    write_u32(cache, pokedex_tiles.size());
    cache.insert(
        cache.end(), pokedex_tiles.begin(),
        pokedex_tiles.end());

    for (const std::uint16_t image :
         {title.logo_image, title.copyright_image, title.version_image,
          title.player_image, title.ball_image})
        write_u16(cache, image);
    write_u16(cache, title.species.size());
    for (const BootTitleSpecies& species : title.species) {
        cache.push_back(species.internal_id);
        write_u16(cache, species.image);
    }
    cache.push_back(static_cast<std::uint8_t>(title.logo_bounce.size()));
    for (const BootTitleBounce& bounce : title.logo_bounce) {
        cache.push_back(static_cast<std::uint8_t>(bounce.pixels_per_frame));
        cache.push_back(bounce.frame_count);
    }
    const auto write_bytes = [&](const std::vector<std::uint8_t>& values) {
        cache.push_back(static_cast<std::uint8_t>(values.size()));
        cache.insert(cache.end(), values.begin(), values.end());
    };
    write_bytes(title.pokemon_scroll_in);
    write_bytes(title.pokemon_scroll_out);
    write_bytes(title.ball_y_positions);
    write_u16(cache, title.setup_frames);
    write_u16(cache, title.after_logo_delay_frames);
    write_u16(cache, title.version_scroll_frames);
    write_u16(cache, title.interruption_wait_frames);

    if (!write_string(cache, menu.continue_label, error) ||
        !write_string(cache, menu.new_game_label, error) ||
        !write_string(cache, menu.option_label, error))
        return false;
    for (const std::string& row : menu.option_rows)
        if (!write_string(cache, row, error)) return false;
    if (!write_string(cache, menu.option_cancel, error)) return false;
    cache.push_back(menu.before_input_delay_frames);

    for (const std::uint16_t image : oak.picture_images) write_u16(cache, image);
    for (const BootTextProgram& text : oak.texts)
        if (!write_text(cache, text, error)) return false;
    for (const std::string& name : oak.player_names)
        if (!write_string(cache, name, error)) return false;
    for (const std::string& name : oak.rival_names)
        if (!write_string(cache, name, error)) return false;
    cache.insert(cache.end(), oak.fade_palettes.begin(), oak.fade_palettes.end());
    cache.push_back(oak.fade_step_delay_frames);
    cache.push_back(oak.slide_step_delay_frames);
    cache.push_back(oak.slide_steps);
    cache.insert(cache.end(), oak.ending_delay_frames.begin(), oak.ending_delay_frames.end());

    // These are the semantic result of the verified NewGame routine. They are
    // importer output, never runtime campaign constants.
    cache.push_back(0x26U);
    cache.push_back(3U);
    cache.push_back(6U);
    cache.push_back(0x00U);
    result.files.push_back({"compiled/boot_content.bin", std::move(cache)});
    return true;
}

} // namespace

bool decode_boot_import(std::span<const std::uint8_t> rom, BootImport& result,
                        std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;

    std::vector<BootImage> images;
    std::vector<std::uint8_t> ui_tiles;
    std::vector<std::uint8_t> pokedex_tiles;
    BootTitleDefinition title;
    BootMenuDefinition menu;
    BootOakDefinition oak;
    std::ostringstream source;
    source << "; Normalized cartridge-derived boot content. Runtime consumes the\n"
           << "; compiled cache and contains no Pokemon Red startup tables.\n\n";
    if (!build_ui_tiles(rom, ui_tiles, error) ||
        !build_pokedex_tiles(rom, pokedex_tiles, error) ||
        !build_title(rom, images, title, source, error) ||
        !build_menu(rom, menu, source, error) ||
        !build_oak(rom, images, oak, source, error) ||
        !emit_cache(
            images, ui_tiles, pokedex_tiles,
            title, menu, oak, result, error))
        return false;

    source << "new_game_state pokemon_red_new_game\n"
           << "    start reds_house_2f at 3 6\n"
           << "    previous_map pallet_town\n\n";
    add_file(result, "source/boot/boot.sexpr", source.str());
    std::ostringstream report;
    report << "Pokemon Red US Rev 0 boot import\n"
           << "images " << images.size() << '\n'
           << "title_species " << title.species.size() << '\n'
           << "oak_text_programs " << oak.texts.size() << '\n'
           << "player_default_names " << oak.player_names.size() << '\n'
           << "rival_default_names " << oak.rival_names.size() << '\n'
           << "runtime_cache compiled/boot_content.bin\n";
    add_file(result, "reports/boot_import_summary.txt", report.str());
    result.images = images.size();
    result.title_species = title.species.size();
    result.text_programs = oak.texts.size();
    error.clear();
    return true;
}

} // namespace pokered::import
