#include "gen1_picture_codec.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

class BitReader {
  public:
    explicit BitReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    bool read_bit(std::uint8_t& value, std::string& error) {
        if (position_ >= bytes_.size() * 8U) {
            error = "compressed picture ends inside its bitstream";
            return false;
        }
        const std::size_t byte_index = position_ / 8U;
        const unsigned shift = static_cast<unsigned>(7U - position_ % 8U);
        value =
            static_cast<std::uint8_t>((static_cast<unsigned>(bytes_[byte_index]) >> shift) & 1U);
        ++position_;
        return true;
    }

    bool read_bits(unsigned count, std::uint16_t& value, std::string& error) {
        value = 0;
        for (unsigned index = 0; index < count; ++index) {
            std::uint8_t bit = 0;
            if (!read_bit(bit, error)) return false;
            value = static_cast<std::uint16_t>(static_cast<std::uint16_t>(value << 1U) | bit);
        }
        return true;
    }

    [[nodiscard]] std::size_t position() const {
        return position_;
    }

  private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_{};
};

bool read_plane(BitReader& reader, std::uint8_t width, std::vector<std::uint8_t>& plane,
                std::string& error) {
    constexpr std::array<std::uint16_t, 16> run_bases = {
        0x0001, 0x0003, 0x0007, 0x000F, 0x001F, 0x003F, 0x007F, 0x00FF,
        0x01FF, 0x03FF, 0x07FF, 0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF,
    };
    const std::size_t group_count = static_cast<std::size_t>(width) * width * 32U;
    std::vector<std::uint8_t> groups;
    groups.reserve(group_count);

    std::uint8_t mode = 0;
    if (!reader.read_bit(mode, error)) return false;
    while (groups.size() < group_count) {
        if (mode != 0) {
            while (groups.size() < group_count) {
                std::uint16_t group = 0;
                if (!reader.read_bits(2, group, error)) return false;
                if (group == 0) break;
                groups.push_back(static_cast<std::uint8_t>(group));
            }
        } else {
            std::size_t width_bits = 0;
            std::uint8_t continuation = 0;
            do {
                if (!reader.read_bit(continuation, error)) return false;
                if (continuation != 0) ++width_bits;
            } while (continuation != 0 && width_bits < run_bases.size());
            if (continuation != 0 || width_bits >= run_bases.size()) {
                error = "compressed picture contains an invalid zero-run prefix";
                return false;
            }
            std::uint16_t suffix = 0;
            if (!reader.read_bits(static_cast<unsigned>(width_bits + 1U), suffix, error))
                return false;
            const std::size_t run = static_cast<std::size_t>(run_bases[width_bits]) + suffix;
            if (run > group_count - groups.size()) {
                error = "compressed picture zero run exceeds its plane";
                return false;
            }
            groups.insert(groups.end(), run, 0);
        }
        mode ^= 1U;
    }

    const std::size_t row_bytes = static_cast<std::size_t>(width) * 8U;
    plane.clear();
    plane.reserve(static_cast<std::size_t>(width) * row_bytes);
    for (std::size_t y = 0; y < width; ++y) {
        for (std::size_t x = 0; x < row_bytes; ++x) {
            std::uint8_t value = 0;
            for (std::size_t group = 0; group < 4; ++group) {
                const std::size_t source = (y * 4U + group) * row_bytes + x;
                value = static_cast<std::uint8_t>(static_cast<std::uint8_t>(value << 2U) |
                                                  groups[source]);
            }
            plane.push_back(value);
        }
    }
    return true;
}

void untransform_plane(std::vector<std::uint8_t>& plane, std::uint8_t width) {
    constexpr std::array<std::array<std::uint8_t, 16>, 2> codes = {{
        {{0x0, 0x1, 0x3, 0x2, 0x7, 0x6, 0x4, 0x5, 0xF, 0xE, 0xC, 0xD, 0x8, 0x9, 0xB, 0xA}},
        {{0xF, 0xE, 0xC, 0xD, 0x8, 0x9, 0xB, 0xA, 0x0, 0x1, 0x3, 0x2, 0x7, 0x6, 0x4, 0x5}},
    }};
    const std::size_t row_bytes = static_cast<std::size_t>(width) * 8U;
    for (std::size_t x = 0; x < row_bytes; ++x) {
        std::uint8_t state = 0;
        for (std::size_t y = 0; y < width; ++y) {
            const std::size_t index = y * row_bytes + x;
            const std::uint8_t high = static_cast<std::uint8_t>(plane[index] >> 4U);
            const std::uint8_t decoded_high = codes[state][high];
            state = static_cast<std::uint8_t>(decoded_high & 1U);
            const std::uint8_t low = static_cast<std::uint8_t>(plane[index] & 0x0FU);
            const std::uint8_t decoded_low = codes[state][low];
            state = static_cast<std::uint8_t>(decoded_low & 1U);
            plane[index] = static_cast<std::uint8_t>(static_cast<std::uint8_t>(decoded_high << 4U) |
                                                     decoded_low);
        }
    }
}

void transpose_tiles(std::vector<std::uint8_t>& bytes, std::uint8_t width) {
    const std::size_t tile_count = static_cast<std::size_t>(width) * width;
    for (std::size_t index = 0; index < tile_count; ++index) {
        const std::size_t transpose = (index * width + index / width) % tile_count;
        if (index >= transpose) continue;
        for (std::size_t byte = 0; byte < 16; ++byte)
            std::swap(bytes[index * 16U + byte], bytes[transpose * 16U + byte]);
    }
}

} // namespace

bool decode_gen1_picture(std::span<const std::uint8_t> compressed, DecodedGen1Picture& output,
                         std::string& error) {
    if (compressed.empty()) {
        error = "compressed picture is empty";
        return false;
    }

    BitReader reader(compressed);
    std::uint16_t width_value = 0;
    std::uint16_t height_value = 0;
    if (!reader.read_bits(4, width_value, error) || !reader.read_bits(4, height_value, error))
        return false;
    if (width_value == 0 || width_value > 15 || height_value != width_value) {
        error = "compressed picture dimensions are not a valid square";
        return false;
    }
    const auto width = static_cast<std::uint8_t>(width_value);

    std::uint8_t order = 0;
    if (!reader.read_bit(order, error)) return false;
    std::array<std::vector<std::uint8_t>, 2> planes;
    if (!read_plane(reader, width, planes[order], error)) return false;

    std::uint8_t mode = 0;
    if (!reader.read_bit(mode, error)) return false;
    if (mode != 0) {
        std::uint8_t second_mode_bit = 0;
        if (!reader.read_bit(second_mode_bit, error)) return false;
        mode = static_cast<std::uint8_t>(mode + second_mode_bit);
    }
    if (!read_plane(reader, width, planes[order ^ 1U], error)) return false;

    untransform_plane(planes[order], width);
    if (mode != 1) untransform_plane(planes[order ^ 1U], width);
    if (mode != 0) {
        for (std::size_t index = 0; index < planes[0].size(); ++index)
            planes[order ^ 1U][index] ^= planes[order][index];
    }

    DecodedGen1Picture decoded;
    decoded.width_tiles = width;
    decoded.height_tiles = width;
    decoded.bits_consumed = reader.position();
    decoded.two_bpp_bytes.reserve(planes[0].size() * 2U);
    for (std::size_t index = 0; index < planes[0].size(); ++index) {
        decoded.two_bpp_bytes.push_back(planes[0][index]);
        decoded.two_bpp_bytes.push_back(planes[1][index]);
    }
    transpose_tiles(decoded.two_bpp_bytes, width);
    output = std::move(decoded);
    error.clear();
    return true;
}

} // namespace pokered::import
