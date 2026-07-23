#include "import_trainers.hpp"

#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

constexpr std::size_t trainer_count = 47U;
constexpr std::size_t presentation_offset = 0x39914U;
constexpr std::size_t names_offset = 0x399FFU;
constexpr std::size_t names_end = 0x39B87U;
constexpr std::size_t party_pointers_offset = 0x39D3BU;
constexpr std::size_t party_data_offset = 0x39D99U;
constexpr std::size_t party_data_end = 0x3A52EU;
constexpr std::size_t ai_offset = 0x3A55CU;
constexpr std::size_t pokedex_order_offset = 0x41024U;
constexpr std::size_t trainer_actor_decode_offset = 0x39C62U;

struct Member {
    std::uint8_t level{};
    std::uint8_t species_dex{};
};

struct Party {
    std::vector<Member> members;
};

struct Trainer {
    std::uint8_t id{};
    std::string name;
    std::uint32_t base_reward{};
    std::uint8_t ai_uses{};
    std::vector<Party> parties;
};

std::uint16_t read_u16(std::span<const std::uint8_t> rom,
                       std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(rom[offset]) |
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(rom[offset + 1U]) << 8U));
}

void write_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void write_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (std::uint8_t shift = 0U; shift < 32U; shift += 8U)
        bytes.push_back(
            static_cast<std::uint8_t>((value >> shift) & 0xFFU));
}

void write_string(std::vector<std::uint8_t>& bytes,
                  const std::string& value) {
    bytes.push_back(static_cast<std::uint8_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
}

void add_text(TrainerImport& result, std::string path,
              const std::string& text) {
    GeneratedFile file;
    file.relative_path = std::move(path);
    file.bytes.assign(text.begin(), text.end());
    result.files.push_back(std::move(file));
}

bool append_character(std::uint8_t value, std::string& text) {
    if (value >= 0x80U && value <= 0x99U) {
        text.push_back(
            static_cast<char>('A' + value - 0x80U));
        return true;
    }
    if (value >= 0xA0U && value <= 0xB9U) {
        text.push_back(
            static_cast<char>('a' + value - 0xA0U));
        return true;
    }
    if (value >= 0xF6U) {
        text.push_back(
            static_cast<char>('0' + value - 0xF6U));
        return true;
    }
    if (value == 0x7FU) {
        text.push_back(' ');
        return true;
    }
    if (value == 0x9AU) {
        text.push_back('(');
        return true;
    }
    if (value == 0x9BU) {
        text.push_back(')');
        return true;
    }
    if (value == 0x9CU) {
        text.push_back(':');
        return true;
    }
    if (value == 0xBAU) {
        text += "é";
        return true;
    }
    if (value == 0xE0U) {
        text.push_back('\'');
        return true;
    }
    if (value == 0xE3U) {
        text.push_back('-');
        return true;
    }
    if (value == 0xE6U) {
        text.push_back('?');
        return true;
    }
    if (value == 0xE7U) {
        text.push_back('!');
        return true;
    }
    if (value == 0xE8U) {
        text.push_back('.');
        return true;
    }
    if (value == 0xEFU) {
        text += "♂";
        return true;
    }
    if (value == 0xF1U) {
        text += "×";
        return true;
    }
    if (value == 0xF5U) {
        text += "♀";
        return true;
    }
    return false;
}

bool decode_name(std::span<const std::uint8_t> rom,
                 std::size_t& cursor, std::string& result,
                 std::string& error) {
    result.clear();
    while (cursor < names_end) {
        const std::uint8_t value = rom[cursor++];
        if (value == 0x50U) return !result.empty();
        if (!append_character(value, result)) {
            error = "trainer name contains unsupported byte " +
                    std::to_string(value);
            return false;
        }
    }
    error = "trainer names are missing a terminator";
    return false;
}

bool pointer_offset(std::span<const std::uint8_t> rom,
                    std::size_t source, std::size_t& result) {
    const std::uint16_t pointer = read_u16(rom, source);
    if (pointer < 0x4000U || pointer >= 0x8000U) return false;
    result = 14U * 0x4000U +
             static_cast<std::size_t>(pointer - 0x4000U);
    return true;
}

bool decode_reward(std::span<const std::uint8_t> rom,
                   std::size_t offset, std::uint32_t& result) {
    result = 0U;
    for (std::size_t index = 0U; index < 3U; ++index) {
        const std::uint8_t packed = rom[offset + index];
        const std::uint8_t high =
            static_cast<std::uint8_t>(packed >> 4U);
        const std::uint8_t low =
            static_cast<std::uint8_t>(packed & 0x0FU);
        if (high > 9U || low > 9U) return false;
        result = result * 100U +
                 static_cast<std::uint32_t>(high) * 10U + low;
    }
    return true;
}

bool decode_party(std::span<const std::uint8_t> rom,
                  const std::array<std::uint8_t, 190>& dex_by_internal,
                  std::size_t& cursor, std::size_t end, Party& party,
                  std::string& error) {
    if (cursor >= end) return false;
    const std::uint8_t first = rom[cursor++];
    const bool uniform = first != 0xFFU;
    while (cursor < end) {
        std::uint8_t level = first;
        if (!uniform) {
            level = rom[cursor++];
            if (level == 0U) break;
            if (cursor >= end) return false;
        }
        const std::uint8_t internal = rom[cursor++];
        if (uniform && internal == 0U) break;
        const std::uint8_t dex =
            internal == 0U || internal > dex_by_internal.size()
                ? 0U
                : dex_by_internal[internal - 1U];
        if (level == 0U || level > 100U || dex == 0U) {
            error = "trainer party has an invalid level or species";
            return false;
        }
        party.members.push_back({level, dex});
    }
    if (party.members.empty() || party.members.size() > 6U ||
        cursor > end || rom[cursor - 1U] != 0U) {
        error = "trainer party is empty, oversized, or unterminated";
        return false;
    }
    return true;
}

} // namespace

bool decode_trainer_import(std::span<const std::uint8_t> rom,
                           TrainerImport& result, std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;
    if (party_data_end > rom.size() ||
        ai_offset + trainer_count * 3U > rom.size() ||
        pokedex_order_offset + 190U > rom.size()) {
        error = "trainer source tables exceed the verified cartridge";
        return false;
    }

    std::array<std::uint8_t, 190> dex_by_internal{};
    for (std::size_t index = 0U; index < dex_by_internal.size(); ++index)
        dex_by_internal[index] = rom[pokedex_order_offset + index];
    if (trainer_actor_decode_offset + 2U > rom.size() ||
        rom[trainer_actor_decode_offset] != 0xD6U ||
        rom[trainer_actor_decode_offset + 1U] == 0U) {
        error =
            "trainer actor decoder does not contain its expected subtract";
        return false;
    }
    const std::uint8_t actor_trainer_offset =
        static_cast<std::uint8_t>(
            rom[trainer_actor_decode_offset + 1U] - 1U);

    std::vector<Trainer> trainers;
    trainers.reserve(trainer_count);
    std::size_t name_cursor = names_offset;
    for (std::size_t index = 0U; index < trainer_count; ++index) {
        Trainer trainer;
        trainer.id = static_cast<std::uint8_t>(index + 1U);
        if (!decode_name(rom, name_cursor, trainer.name, error) ||
            !decode_reward(rom, presentation_offset + index * 5U + 2U,
                           trainer.base_reward)) {
            if (error.empty()) error = "trainer reward is invalid BCD";
            return false;
        }
        const std::size_t ai = ai_offset + index * 3U;
        trainer.ai_uses = rom[ai];
        std::size_t ignored = 0U;
        if (trainer.ai_uses == 0U || trainer.ai_uses > 5U ||
            !pointer_offset(rom, ai + 1U, ignored)) {
            error = "trainer class has an invalid AI binding";
            return false;
        }

        std::size_t cursor = 0U;
        std::size_t end = party_data_end;
        const std::size_t pointer =
            party_pointers_offset + index * 2U;
        if (!pointer_offset(rom, pointer, cursor) ||
            (index + 1U < trainer_count &&
             !pointer_offset(rom, pointer + 2U, end)) ||
            cursor < party_data_offset || end < cursor ||
            end > party_data_end) {
            error = "trainer party pointer is outside its class range";
            return false;
        }
        while (cursor < end) {
            Party party;
            if (!decode_party(rom, dex_by_internal, cursor, end, party,
                              error))
                return false;
            result.members += party.members.size();
            trainer.parties.push_back(std::move(party));
        }
        result.parties += trainer.parties.size();
        trainers.push_back(std::move(trainer));
    }
    if (name_cursor != names_end) {
        error = "trainer names do not consume their exact source range";
        return false;
    }

    std::ostringstream source;
    source << "; Cartridge-derived actor encoding, trainer classes, and indexed parties.\n\n"
           << "actor_opponent_policy\n"
           << "    trainer_class_offset "
           << static_cast<unsigned>(actor_trainer_offset) << '\n'
           << "    trainer_party_index one_based\n"
           << "    static_species_index internal_species\n"
           << "    static_level parameter_b\n\n"
           << "internal_species_to_dex\n";
    for (std::size_t index = 0U; index < dex_by_internal.size();
         ++index)
        source << "    slot " << index + 1U << " dex "
               << static_cast<unsigned>(dex_by_internal[index]) << '\n';
    source << '\n';
    for (const Trainer& trainer : trainers) {
        source << "trainer_class "
               << static_cast<unsigned>(trainer.id) << '\n'
               << "    name \"" << trainer.name << "\"\n"
               << "    base_reward " << trainer.base_reward << '\n'
               << "    ai_uses "
               << static_cast<unsigned>(trainer.ai_uses) << '\n';
        for (std::size_t party_index = 0U;
             party_index < trainer.parties.size(); ++party_index) {
            source << "    party " << party_index << '\n';
            for (const Member& member :
                 trainer.parties[party_index].members)
                source << "        member species_"
                       << static_cast<unsigned>(member.species_dex)
                       << " level "
                       << static_cast<unsigned>(member.level) << '\n';
        }
        source << '\n';
    }
    add_text(result, "source/trainers/classes_and_parties.sexpr",
             source.str());

    std::vector<std::uint8_t> cache{'P', 'T', 'C', '2'};
    cache.push_back(actor_trainer_offset);
    cache.insert(cache.end(), dex_by_internal.begin(),
                 dex_by_internal.end());
    cache.push_back(static_cast<std::uint8_t>(trainers.size()));
    for (const Trainer& trainer : trainers) {
        cache.push_back(trainer.id);
        write_string(cache, trainer.name);
        write_u32(cache, trainer.base_reward);
        cache.push_back(trainer.ai_uses);
        write_u16(cache,
                  static_cast<std::uint16_t>(trainer.parties.size()));
        for (const Party& party : trainer.parties) {
            cache.push_back(
                static_cast<std::uint8_t>(party.members.size()));
            for (const Member& member : party.members) {
                cache.push_back(member.level);
                cache.push_back(member.species_dex);
            }
        }
    }
    result.files.push_back(
        {"compiled/trainers.bin", std::move(cache)});

    std::ostringstream report;
    report << "Pokemon Red trainer import\n"
           << "actor_trainer_offset "
           << static_cast<unsigned>(actor_trainer_offset) << '\n'
           << "internal_species_slots " << dex_by_internal.size() << '\n'
           << "trainer_classes " << trainers.size() << '\n'
           << "trainer_parties " << result.parties << '\n'
           << "party_members " << result.members << '\n';
    add_text(result, "reports/trainer_import_summary.txt", report.str());
    result.classes = trainers.size();
    error.clear();
    return true;
}

} // namespace pokered::import
