#include "trainers.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <utility>

namespace pokered {
namespace {

bool read_u8(std::istream& input, std::uint8_t& result) {
    char byte = 0;
    if (!input.get(byte)) return false;
    result =
        static_cast<std::uint8_t>(static_cast<unsigned char>(byte));
    return true;
}

bool read_u16(std::istream& input, std::uint16_t& result) {
    std::uint8_t low = 0;
    std::uint8_t high = 0;
    if (!read_u8(input, low) || !read_u8(input, high)) return false;
    result = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(low) |
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(high) << 8U));
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

bool read_string(std::istream& input, std::string& result) {
    std::uint8_t size = 0;
    if (!read_u8(input, size) || size == 0U || size > 63U)
        return false;
    result.resize(size);
    return input
        .read(result.data(), static_cast<std::streamsize>(size))
        .good();
}

} // namespace

bool load_trainers(const std::filesystem::path& path,
                   TrainerCatalog& result, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    std::uint8_t class_count = 0;
    if (!input.read(magic.data(),
                    static_cast<std::streamsize>(magic.size())) ||
        magic != std::array{'P', 'T', 'C', '2'}) {
        error = "trainer cache is missing or has an invalid header";
        return false;
    }

    TrainerCatalog loaded;
    loaded.source = path;
    if (!read_u8(input, loaded.actor_trainer_offset) ||
        loaded.actor_trainer_offset == 0U ||
        !input.read(
            reinterpret_cast<char*>(loaded.dex_by_internal.data()),
            static_cast<std::streamsize>(loaded.dex_by_internal.size())) ||
        !read_u8(input, class_count) || class_count == 0U ||
        class_count >
            static_cast<std::uint8_t>(
                0xFFU - loaded.actor_trainer_offset)) {
        error = "trainer cache has an invalid actor-opponent policy";
        return false;
    }
    const auto species_count = std::ranges::count_if(
        loaded.dex_by_internal,
        [](std::uint8_t dex) { return dex != 0U && dex <= 151U; });
    const bool invalid_species = std::ranges::any_of(
        loaded.dex_by_internal,
        [](std::uint8_t dex) { return dex > 151U; });
    if (species_count != 151U || invalid_species) {
        error =
            "trainer cache has an invalid internal-species mapping";
        return false;
    }
    loaded.classes.reserve(class_count);
    for (std::uint8_t index = 0U; index < class_count; ++index) {
        TrainerClassRule trainer;
        std::uint16_t party_count = 0;
        if (!read_u8(input, trainer.id) ||
            trainer.id != static_cast<std::uint8_t>(index + 1U) ||
            !read_string(input, trainer.name) ||
            !read_u32(input, trainer.base_reward) ||
            !read_u8(input, trainer.ai_uses) ||
            trainer.ai_uses == 0U || trainer.ai_uses > 5U ||
            !read_u16(input, party_count) || party_count > 256U) {
            error = "trainer cache has an invalid class record at index " +
                    std::to_string(index);
            return false;
        }
        trainer.parties.reserve(party_count);
        for (std::uint16_t party_index = 0U;
             party_index < party_count; ++party_index) {
            TrainerPartyRule party;
            std::uint8_t member_count = 0;
            if (!read_u8(input, member_count) || member_count == 0U ||
                member_count > 6U) {
                error = "trainer cache has an invalid party size";
                return false;
            }
            party.members.reserve(member_count);
            for (std::uint8_t member_index = 0U;
                 member_index < member_count; ++member_index) {
                TrainerPartyMember member;
                if (!read_u8(input, member.level) ||
                    !read_u8(input, member.species_dex) ||
                    member.level == 0U || member.level > 100U ||
                    member.species_dex == 0U ||
                    member.species_dex > 151U) {
                    error = "trainer cache has an invalid party member";
                    return false;
                }
                party.members.push_back(member);
            }
            trainer.parties.push_back(std::move(party));
        }
        loaded.classes.push_back(std::move(trainer));
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "trainer cache contains trailing data";
        return false;
    }
    loaded.loaded = true;
    result = std::move(loaded);
    error.clear();
    return true;
}

const TrainerClassRule* find_trainer_class(
    const TrainerCatalog& trainers, std::uint8_t class_id) {
    return class_id != 0U && class_id <= trainers.classes.size() &&
                   trainers.classes[class_id - 1U].id == class_id
               ? &trainers.classes[class_id - 1U]
               : nullptr;
}

const TrainerPartyRule* find_trainer_party(
    const TrainerCatalog& trainers, std::uint8_t class_id,
    std::uint16_t party_index) {
    const TrainerClassRule* trainer =
        find_trainer_class(trainers, class_id);
    return trainer != nullptr && party_index < trainer->parties.size()
               ? &trainer->parties[party_index]
               : nullptr;
}

bool resolve_actor_opponent(const TrainerCatalog& trainers,
                            std::uint8_t parameter_a,
                            std::uint8_t parameter_b,
                            ActorOpponentBinding& result,
                            std::string& error) {
    if (!trainers.loaded) {
        error = "actor opponent requires a loaded trainer catalog";
        return false;
    }

    // The imported policy separates trainer-class IDs from internal species
    // slots. Both compact cartridge encodings become semantic runtime IDs here.
    if (parameter_a > trainers.actor_trainer_offset) {
        const std::uint8_t class_id = static_cast<std::uint8_t>(
            parameter_a - trainers.actor_trainer_offset);
        if (parameter_b == 0U ||
            find_trainer_party(trainers, class_id,
                               static_cast<std::uint16_t>(
                                   parameter_b - 1U)) == nullptr) {
            error = "actor references an unavailable trainer party";
            return false;
        }
        result = {
            .kind = ActorOpponentKind::trainer,
            .trainer_class_id = class_id,
            .trainer_party_index =
                static_cast<std::uint16_t>(parameter_b - 1U),
        };
        error.clear();
        return true;
    }

    if (parameter_a == 0U ||
        parameter_a > trainers.dex_by_internal.size() ||
        parameter_b == 0U || parameter_b > 100U) {
        error = "actor has an invalid static-Pokemon encoding";
        return false;
    }
    const std::uint8_t species_dex =
        trainers.dex_by_internal[parameter_a - 1U];
    if (species_dex == 0U) {
        error = "actor references an unused internal species slot";
        return false;
    }
    result = {
        .kind = ActorOpponentKind::static_pokemon,
        .species_dex = species_dex,
        .level = parameter_b,
    };
    error.clear();
    return true;
}

} // namespace pokered
