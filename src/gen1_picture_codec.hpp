#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct DecodedGen1Picture {
    std::uint8_t width_tiles{};
    std::uint8_t height_tiles{};
    std::size_t bits_consumed{};
    std::vector<std::uint8_t> two_bpp_bytes;
};

// Decode one picture at the beginning of a larger ROM span and report its exact
// consumed bit count. Callers retain ownership of locating and validating the
// cartridge record.
bool decode_gen1_picture(std::span<const std::uint8_t> compressed, DecodedGen1Picture& output,
                         std::string& error);

} // namespace pokered::import
