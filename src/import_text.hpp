#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace pokered::import {

struct DecodedTextProgram {
    std::string operations;
    std::size_t source_bytes{};
    bool complete{};
    bool dynamic{};
    std::string unresolved_reason;
};

// Decode Gen 1's bounded text-command and character data languages.
bool decode_text_program(std::span<const std::uint8_t> rom, std::uint8_t bank, std::size_t offset,
                         DecodedTextProgram& result);

} // namespace pokered::import
