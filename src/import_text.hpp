#pragma once

#include "interaction_kinds.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct DecodedTextProgram {
    std::string operations;
    // Presentation-ready pages retained beside the readable instruction stream.
    std::vector<std::string> pages;
    std::size_t source_bytes{};
    bool complete{};
    bool dynamic{};
    bool interaction{};
    InteractionBuiltin builtin{InteractionBuiltin::none};
    std::vector<std::uint16_t> item_ids;
    std::string unresolved_reason;
};

// Decode Gen 1's bounded text-command and character data languages.
std::string decode_text_glyph(std::uint8_t value);
bool decode_text_program(std::span<const std::uint8_t> rom, std::uint8_t bank, std::size_t offset,
                         DecodedTextProgram& result);

} // namespace pokered::import
