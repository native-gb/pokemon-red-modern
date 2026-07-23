#include "campaign_programs.hpp"

#include "battle_rules.hpp"
#include "maps.hpp"
#include "pokemon.hpp"
#include "rules.hpp"
#include "state.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>

namespace pokered {
namespace {

bool read_u8(std::istream& input, std::uint8_t& result) {
    char byte = 0;
    if (!input.get(byte)) return false;
    result = static_cast<std::uint8_t>(static_cast<unsigned char>(byte));
    return true;
}

bool read_u16(std::istream& input, std::uint16_t& result) {
    std::uint8_t low = 0U;
    std::uint8_t high = 0U;
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
    result = static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8U |
             static_cast<std::uint32_t>(bytes[2]) << 16U |
             static_cast<std::uint32_t>(bytes[3]) << 24U;
    return true;
}

bool read_string(std::istream& input, std::string& result) {
    std::uint16_t size = 0U;
    if (!read_u16(input, size) || size == 0U || size > 8192U) return false;
    result.resize(size);
    return input.read(result.data(), static_cast<std::streamsize>(size)).good();
}

bool read_pages(std::istream& input, std::vector<std::string>& result) {
    std::uint16_t count = 0U;
    if (!read_u16(input, count) || count > 64U) return false;
    result.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        std::string page;
        if (!read_string(input, page)) return false;
        result.push_back(std::move(page));
    }
    return true;
}

bool read_path(std::istream& input, std::vector<WorldPathCommand>& result) {
    std::uint16_t count = 0U;
    if (!read_u16(input, count) || count > 1024U) return false;
    result.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        std::uint8_t command = 0U;
        if (!read_u8(input, command) ||
            command > static_cast<std::uint8_t>(WorldPathCommand::face_down))
            return false;
        result.push_back(static_cast<WorldPathCommand>(command));
    }
    return true;
}

WorldDirection direction(std::uint8_t encoded) {
    if (encoded == 1U) return WorldDirection::up;
    if (encoded == 2U) return WorldDirection::left;
    if (encoded == 3U) return WorldDirection::right;
    return WorldDirection::down;
}

bool valid_instruction(const CampaignInstruction& instruction) {
    const auto owns_actor = [&]() {
        return instruction.a != 0U && instruction.value <= 0xFFU;
    };
    switch (instruction.opcode) {
    case CampaignOpcode::show_actor:
    case CampaignOpcode::hide_actor:
    case CampaignOpcode::move_actor_to_player:
    case CampaignOpcode::align_pair_x:
        return owns_actor();
    case CampaignOpcode::face_actor:
        return owns_actor() && instruction.b <= 3U;
    case CampaignOpcode::face_player:
        return instruction.a <= 3U;
    case CampaignOpcode::say:
        return !instruction.pages.empty();
    case CampaignOpcode::ask_yes_no:
        return !instruction.pages.empty();
    case CampaignOpcode::parallel_path:
        return owns_actor() && instruction.b <= 1U &&
               !instruction.actor_path.empty() &&
               !instruction.player_path.empty();
    case CampaignOpcode::lock_input:
    case CampaignOpcode::set_flag:
    case CampaignOpcode::end_if_choice_no:
    case CampaignOpcode::unlock_input:
    case CampaignOpcode::end:
        return true;
    case CampaignOpcode::set_variable:
        return instruction.a < 64U && instruction.value <= 0xFFFFU;
    case CampaignOpcode::give_pokemon:
        return instruction.a != 0U && instruction.a <= 100U &&
               instruction.value != 0U && instruction.value <= 0xFFU;
    }
    return false;
}

bool trigger_ready(const CampaignProgram& program, const WorldState& world,
                   const CampaignState& campaign) {
    if (!world.player.initialized ||
        (program.required_flag != 0xFFFFFFFFU &&
         !campaign_flag(campaign, program.required_flag)) ||
        (program.absent_flag != 0xFFFFFFFFU &&
         campaign_flag(campaign, program.absent_flag)))
        return false;
    if (program.trigger_kind == CampaignTriggerKind::actor_activation)
        return world.last_actor_activation.occurred &&
               world.last_actor_activation.map_id ==
                   program.trigger_map_id &&
               world.last_actor_activation.actor_index ==
                   program.trigger_value;
    return world.player.map_index < world.maps.size() &&
           world.maps[world.player.map_index].id ==
               program.trigger_map_id &&
           world.player.y == program.trigger_value;
}

void replace_all(std::string& text, std::string_view token,
                 std::string_view value) {
    std::size_t position = 0U;
    while ((position = text.find(token, position)) != std::string::npos) {
        text.replace(position, token.size(), value);
        position += value.size();
    }
}

void open_program_dialogue(WorldState& world,
                           const CampaignState& campaign,
                           const RuleCatalog& rules,
                           const CampaignInstruction& instruction) {
    std::vector<std::string> pages = instruction.pages;
    if (instruction.value != 0U) {
        const SpeciesRule* species = find_species(
            rules, static_cast<std::uint8_t>(instruction.value));
        if (species != nullptr) {
            for (std::string& page : pages) {
                replace_all(page, "{species_name}", species->name);
                replace_all(page, "{name_buffer}", species->name);
            }
        }
    }
    open_world_dialogue(world, campaign, pages);
}

} // namespace

bool load_campaign_programs(const std::filesystem::path& path, CampaignProgramCatalog& result,
                            std::string& error) {
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    std::uint16_t program_count = 0U;
    if (!input.read(magic.data(), static_cast<std::streamsize>(magic.size())) ||
        magic != std::array{'P', 'C', 'P', '2'} || !read_u16(input, program_count) ||
        program_count == 0U || program_count > 4096U) {
        error = "campaign program cache has an invalid header";
        return false;
    }

    CampaignProgramCatalog loaded;
    loaded.source = path;
    loaded.programs.reserve(program_count);
    for (std::uint16_t index = 0U; index < program_count; ++index) {
        CampaignProgram program;
        std::uint8_t trigger_kind = 0U;
        std::uint16_t hidden_count = 0U;
        std::uint16_t instruction_count = 0U;
        if (!read_string(input, program.key) ||
            !read_u8(input, trigger_kind) ||
            trigger_kind >
                static_cast<std::uint8_t>(
                    CampaignTriggerKind::actor_activation) ||
            !read_u8(input, program.trigger_map_id) ||
            !read_u8(input, program.trigger_value) ||
            !read_u32(input, program.required_flag) ||
            !read_u32(input, program.absent_flag) ||
            !read_u16(input, hidden_count) || hidden_count > 64U)
            return false;
        program.trigger_kind =
            static_cast<CampaignTriggerKind>(trigger_kind);
        program.initially_hidden.reserve(hidden_count);
        for (std::uint16_t hidden = 0U; hidden < hidden_count; ++hidden) {
            CampaignActorRef actor;
            if (!read_u8(input, actor.map_id) || !read_u8(input, actor.actor_index) ||
                actor.actor_index == 0U)
                return false;
            program.initially_hidden.push_back(actor);
        }
        if (!read_u16(input, instruction_count) || instruction_count == 0U ||
            instruction_count > 4096U)
            return false;
        program.instructions.reserve(instruction_count);
        for (std::uint16_t instruction_index = 0U; instruction_index < instruction_count;
             ++instruction_index) {
            CampaignInstruction instruction;
            std::uint8_t opcode = 0U;
            if (!read_u8(input, opcode) ||
                opcode > static_cast<std::uint8_t>(CampaignOpcode::end) ||
                !read_u8(input, instruction.a) || !read_u8(input, instruction.b) ||
                !read_u32(input, instruction.value) || !read_pages(input, instruction.pages) ||
                !read_path(input, instruction.actor_path) ||
                !read_path(input, instruction.player_path))
                return false;
            instruction.opcode = static_cast<CampaignOpcode>(opcode);
            if (!valid_instruction(instruction)) {
                error =
                    "campaign program cache contains an invalid instruction";
                return false;
            }
            program.instructions.push_back(std::move(instruction));
        }
        if (program.instructions.back().opcode != CampaignOpcode::end) {
            error = "campaign program is missing its end instruction";
            return false;
        }
        loaded.programs.push_back(std::move(program));
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "campaign program cache contains trailing data";
        return false;
    }
    loaded.loaded = true;
    result = std::move(loaded);
    error.clear();
    return true;
}

bool initialize_campaign_program_runtime(const CampaignProgramCatalog& programs, WorldState& world,
                                         std::string& error) {
    if (!programs.loaded || !world.loaded) {
        error = "campaign program runtime requires loaded content";
        return false;
    }
    for (const CampaignProgram& program : programs.programs) {
        for (const CampaignActorRef& actor : program.initially_hidden) {
            if (!set_world_actor_visible(world, actor.map_id, actor.actor_index, false, error))
                return false;
        }
    }
    error.clear();
    return true;
}

bool service_campaign_programs(const CampaignProgramCatalog& programs,
                               const RuleCatalog& rules,
                               const BattleRuleCatalog& battle_rules,
                               WorldState& world, CampaignState& campaign,
                               std::string& error) {
    if (!programs.loaded || !world.loaded || !campaign.initialized) {
        error.clear();
        return true;
    }

    CampaignFiberState& fiber = campaign.fiber;
    if (!fiber.active) {
        for (std::size_t index = 0U; index < programs.programs.size(); ++index) {
            if (!trigger_ready(programs.programs[index], world, campaign)) continue;
            fiber = {
                .program_index = index,
                .instruction_index = 0U,
                .waiting_dialogue = false,
                .waiting_motion = false,
                .waiting_choice = false,
                .last_choice = 0U,
                .active = true,
            };
            break;
        }
        if (!fiber.active) {
            error.clear();
            return true;
        }
    }
    if (fiber.program_index >= programs.programs.size()) {
        error = "campaign fiber lost its program";
        return false;
    }
    const CampaignProgram& program = programs.programs[fiber.program_index];

    if (fiber.waiting_dialogue) {
        if (world.dialogue.open) {
            error.clear();
            return true;
        }
        fiber.waiting_dialogue = false;
    }
    if (fiber.waiting_motion) {
        if (!step_world_script_motion(world, error)) return false;
        if (world.script_motion.active) {
            error.clear();
            return true;
        }
        fiber.waiting_motion = false;
    }
    if (fiber.waiting_choice) {
        if (world.choice.open) {
            error.clear();
            return true;
        }
        if (!world.choice.decided) {
            error = "campaign choice closed without a result";
            return false;
        }
        fiber.last_choice =
            static_cast<std::uint8_t>(world.choice.selected);
        world.choice = {};
        fiber.waiting_choice = false;
    }

    for (std::size_t operations = 0U; operations < 64U; ++operations) {
        if (fiber.instruction_index >= program.instructions.size()) {
            error = "campaign fiber ran beyond its program";
            return false;
        }
        const CampaignInstruction& instruction = program.instructions[fiber.instruction_index++];
        switch (instruction.opcode) {
        case CampaignOpcode::lock_input:
            campaign.input_locked = true;
            break;
        case CampaignOpcode::set_flag:
            set_campaign_flag(campaign, instruction.value, true);
            break;
        case CampaignOpcode::show_actor:
        case CampaignOpcode::hide_actor:
            if (!set_world_actor_visible(world, static_cast<std::uint8_t>(instruction.value),
                                         instruction.a,
                                         instruction.opcode == CampaignOpcode::show_actor, error))
                return false;
            break;
        case CampaignOpcode::say:
            open_program_dialogue(world, campaign, rules, instruction);
            fiber.waiting_dialogue = world.dialogue.open;
            error.clear();
            return true;
        case CampaignOpcode::face_actor:
            if (!face_world_actor(world, static_cast<std::uint8_t>(instruction.value),
                                  instruction.a, direction(instruction.b), error))
                return false;
            break;
        case CampaignOpcode::face_player:
            world.player.facing = direction(instruction.a);
            break;
        case CampaignOpcode::move_actor_to_player:
            if (!start_world_actor_to_player_motion(
                    world, static_cast<std::uint8_t>(instruction.value), instruction.a,
                    static_cast<std::int8_t>(instruction.b), error))
                return false;
            fiber.waiting_motion = true;
            error.clear();
            return true;
        case CampaignOpcode::align_pair_x:
            if (!start_world_pair_alignment(world, static_cast<std::uint8_t>(instruction.value),
                                            instruction.a, instruction.b, error))
                return false;
            fiber.waiting_motion = world.script_motion.active;
            if (fiber.waiting_motion) {
                error.clear();
                return true;
            }
            break;
        case CampaignOpcode::parallel_path:
            if (!start_world_parallel_motion(world, static_cast<std::uint8_t>(instruction.value),
                                             instruction.a, instruction.actor_path,
                                             instruction.player_path, instruction.b != 0U, error))
                return false;
            fiber.waiting_motion = true;
            error.clear();
            return true;
        case CampaignOpcode::ask_yes_no:
            open_program_dialogue(world, campaign, rules, instruction);
            world.choice = {
                .options = {"YES", "NO"},
                .selected = 0U,
                .input_cooldown = 0U,
                .open = true,
                .decided = false,
            };
            fiber.waiting_choice = true;
            error.clear();
            return true;
        case CampaignOpcode::end_if_choice_no:
            if (fiber.last_choice != 0U) {
                campaign.input_locked = false;
                fiber = {};
                error.clear();
                return true;
            }
            break;
        case CampaignOpcode::set_variable:
            set_campaign_variable(
                campaign, instruction.a,
                static_cast<std::uint16_t>(instruction.value));
            break;
        case CampaignOpcode::give_pokemon: {
            if (campaign.party.members.size() >= 6U) {
                error = "campaign cannot add a Pokemon to a full party";
                return false;
            }
            const StatFormulaProgram* stat_formula = find_stat_formula(
                battle_rules, battle_rules.original_stat_formula);
            if (stat_formula == nullptr) {
                error = "campaign Pokemon creation lost its stat formula";
                return false;
            }
            const std::uint8_t first = next_world_random_byte(world);
            const std::uint8_t second = next_world_random_byte(world);
            PokemonState pokemon;
            if (!build_pokemon(
                    rules, *stat_formula,
                    static_cast<std::uint8_t>(instruction.value),
                    instruction.a,
                    {
                        static_cast<std::uint8_t>(first >> 4U),
                        static_cast<std::uint8_t>(first & 0x0FU),
                        static_cast<std::uint8_t>(second >> 4U),
                        static_cast<std::uint8_t>(second & 0x0FU),
                    },
                    campaign.trainer_id, campaign.player_name, pokemon,
                    error))
                return false;
            campaign.party.members.push_back(std::move(pokemon));
            break;
        }
        case CampaignOpcode::unlock_input:
            campaign.input_locked = false;
            break;
        case CampaignOpcode::end:
            campaign.input_locked = false;
            fiber = {};
            error.clear();
            return true;
        }
    }
    error = "campaign fiber exceeded its immediate-operation budget";
    return false;
}

} // namespace pokered
