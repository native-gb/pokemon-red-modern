#include "naming.hpp"

#include <algorithm>

namespace pokered {
namespace {

std::size_t utf8_character_size(unsigned char lead) {
    if ((lead & 0x80U) == 0U) return 1U;
    if ((lead & 0xE0U) == 0xC0U) return 2U;
    if ((lead & 0xF0U) == 0xE0U) return 3U;
    if ((lead & 0xF8U) == 0xF0U) return 4U;
    return 0U;
}

bool valid_utf8_character(std::string_view text, std::size_t cursor,
                          std::size_t size) {
    if (size == 0U || cursor + size > text.size()) return false;
    for (std::size_t index = 1U; index < size; ++index)
        if ((static_cast<unsigned char>(text[cursor + index]) & 0xC0U) !=
            0x80U)
            return false;
    return true;
}

void erase_last_character_impl(std::string& value) {
    if (value.empty()) return;
    std::size_t cursor = value.size() - 1U;
    while (cursor > 0U &&
           (static_cast<unsigned char>(value[cursor]) & 0xC0U) == 0x80U)
        --cursor;
    value.erase(cursor);
}

void append_cell(NamingState& state, const std::string& cell) {
    if (cell == "END" ||
        state.glyphs.size() >= state.profile.maximum_length)
        return;
    state.glyphs.push_back(cell);
    state.value += cell;
}

void erase_last_glyph(NamingState& state) {
    if (state.glyphs.empty()) return;
    const std::size_t size = state.glyphs.back().size();
    if (size <= state.value.size())
        state.value.erase(state.value.size() - size);
    state.glyphs.pop_back();
}

} // namespace

bool valid_naming_profile(const NamingProfile& profile) {
    if (profile.maximum_length == 0U || profile.maximum_length > 32U ||
        profile.uppercase_action.empty() ||
        profile.lowercase_action.empty())
        return false;
    return std::ranges::none_of(
               profile.uppercase,
               [](const std::string& cell) { return cell.empty(); }) &&
           std::ranges::none_of(
               profile.lowercase,
               [](const std::string& cell) { return cell.empty(); }) &&
           profile.uppercase.back() == "END" &&
           profile.lowercase.back() == "END";
}

void begin_naming(const NamingProfile& profile, std::string heading,
                  NamingState& state) {
    state = {
        .profile = profile,
        .heading = std::move(heading),
        .value = {},
        .glyphs = {},
        .row = 0U,
        .column = 0U,
        .input_cooldown = 0U,
        .lowercase = false,
        .open = true,
        .decided = false,
    };
}

void step_naming(const NamingInput& input, NamingState& state) {
    if (!state.open || !valid_naming_profile(state.profile)) return;

    // The modern host keeps the cartridge grid predictable: direction,
    // confirm, and back are the only naming controls. This prevents ordinary
    // gameplay bindings from also becoming text or case-toggle commands.
    if (input.erase) {
        erase_last_glyph(state);
        return;
    }

    if (state.input_cooldown > 0U) {
        --state.input_cooldown;
    } else if (input.left || input.right || input.up || input.down) {
        if (input.up) {
            state.row =
                state.row == 0U
                    ? static_cast<std::uint8_t>(kNamingRows)
                    : static_cast<std::uint8_t>(state.row - 1U);
            if (state.row == kNamingRows) state.column = 0U;
        } else if (input.down) {
            state.row =
                state.row == kNamingRows
                    ? 0U
                    : static_cast<std::uint8_t>(state.row + 1U);
            if (state.row == kNamingRows) state.column = 0U;
        } else if (state.row < kNamingRows && input.left) {
            state.column =
                state.column == 0U
                    ? static_cast<std::uint8_t>(kNamingColumns - 1U)
                    : static_cast<std::uint8_t>(state.column - 1U);
        } else if (state.row < kNamingRows && input.right) {
            state.column = static_cast<std::uint8_t>(
                (state.column + 1U) % kNamingColumns);
        }
        state.input_cooldown = 8U;
    }

    if (!input.confirm) return;
    if (state.row == kNamingRows) {
        state.lowercase = !state.lowercase;
        return;
    }
    const std::string& cell =
        naming_cell(state, state.row, state.column);
    if (cell == "END") {
        state.open = false;
        state.decided = true;
        return;
    }
    append_cell(state, cell);
}

const std::string& naming_cell(const NamingState& state, std::uint8_t row,
                               std::uint8_t column) {
    static const std::string empty;
    if (row >= kNamingRows || column >= kNamingColumns) return empty;
    const std::size_t index =
        static_cast<std::size_t>(row) * kNamingColumns + column;
    return state.lowercase ? state.profile.lowercase[index]
                           : state.profile.uppercase[index];
}

std::size_t naming_character_count(std::string_view value) {
    std::size_t count = 0U;
    for (std::size_t cursor = 0U; cursor < value.size();) {
        const std::size_t size =
            utf8_character_size(static_cast<unsigned char>(value[cursor]));
        if (!valid_utf8_character(value, cursor, size)) {
            ++cursor;
        } else {
            cursor += size;
        }
        ++count;
    }
    return count;
}

void append_typed_naming_text(std::string& value, std::string_view text,
                              std::size_t maximum_length) {
    std::size_t count = naming_character_count(value);
    for (std::size_t cursor = 0U;
         cursor < text.size() && count < maximum_length;) {
        const std::size_t size =
            utf8_character_size(static_cast<unsigned char>(text[cursor]));
        if (!valid_utf8_character(text, cursor, size)) {
            ++cursor;
            continue;
        }
        const unsigned char first =
            static_cast<unsigned char>(text[cursor]);
        if ((size == 1U && first >= 0x20U && first != 0x7FU) ||
            size > 1U) {
            value.append(text.substr(cursor, size));
            ++count;
        }
        cursor += size;
    }
}

void erase_last_naming_character(std::string& value) {
    erase_last_character_impl(value);
}

} // namespace pokered
