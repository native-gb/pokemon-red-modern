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
    case CampaignOpcode::say_if_player_won:
    case CampaignOpcode::say_if_player_lost:
        return !instruction.pages.empty();
    case CampaignOpcode::parallel_path:
        return owns_actor() && instruction.b <= 1U &&
               !instruction.actor_path.empty() &&
               !instruction.player_path.empty();
    case CampaignOpcode::actor_path_by_player_x:
    case CampaignOpcode::actor_path_by_player_y:
        return owns_actor() && !instruction.actor_path.empty() &&
               !instruction.player_path.empty();
    case CampaignOpcode::lock_input:
    case CampaignOpcode::set_flag:
    case CampaignOpcode::clear_flag:
    case CampaignOpcode::end_if_choice_no:
    case CampaignOpcode::end_if_player_lost:
    case CampaignOpcode::heal_party:
    case CampaignOpcode::unlock_input:
    case CampaignOpcode::end:
        return true;
    case CampaignOpcode::set_variable:
        return instruction.a < 64U && instruction.value <= 0xFFFFU;
    case CampaignOpcode::give_pokemon:
        return instruction.a != 0U && instruction.a <= 100U &&
               instruction.value != 0U && instruction.value <= 0xFFU;
    case CampaignOpcode::nickname_last_party_member_if_yes:
        return instruction.a == 0U && instruction.b == 0U &&
               instruction.value == 0U && instruction.pages.empty() &&
               instruction.actor_path.empty() &&
               instruction.player_path.empty();
    case CampaignOpcode::player_path:
        return instruction.a == 0U && instruction.b == 0U &&
               instruction.value == 0U && instruction.pages.empty() &&
               instruction.actor_path.empty() &&
               !instruction.player_path.empty();
    case CampaignOpcode::give_item:
    case CampaignOpcode::try_give_item:
    case CampaignOpcode::take_item:
        return instruction.a != 0U && instruction.b == 0U &&
               instruction.value != 0U &&
               instruction.value <= 0xFFFFU &&
               instruction.pages.empty() &&
               instruction.actor_path.empty() &&
               instruction.player_path.empty();
    case CampaignOpcode::place_actor:
        return instruction.a != 0U &&
               instruction.pages.empty() &&
               instruction.actor_path.empty() &&
               instruction.player_path.empty();
    case CampaignOpcode::actor_path:
        return instruction.a != 0U && instruction.value <= 1U &&
               instruction.pages.empty() &&
               !instruction.actor_path.empty() &&
               instruction.player_path.empty();
    case CampaignOpcode::jump_if_player_y:
    case CampaignOpcode::jump_if_item_grant_failed:
        return instruction.b == 0U && instruction.pages.empty() &&
               instruction.actor_path.empty() &&
               instruction.player_path.empty();
    case CampaignOpcode::jump:
        return instruction.a == 0U && instruction.b == 0U &&
               instruction.pages.empty() &&
               instruction.actor_path.empty() &&
               instruction.player_path.empty();
    case CampaignOpcode::wait_ticks:
        return instruction.value != 0U;
    case CampaignOpcode::start_trainer_battle:
        return instruction.a != 0U && instruction.b == 0U &&
               instruction.value <= 0xFFFFU;
    }
    return false;
}

bool trigger_ready(const CampaignProgram& program, const WorldState& world,
                   const CampaignState& campaign) {
    if (!world.player.initialized ||
        (program.required_flag != 0xFFFFFFFFU &&
         !campaign_flag(campaign, program.required_flag)) ||
        (program.absent_flag != 0xFFFFFFFFU &&
         campaign_flag(campaign, program.absent_flag)) ||
        (program.required_variable != 0xFFFFU &&
         campaign_variable(campaign, program.required_variable) !=
             program.required_variable_value) ||
        (program.required_item_id != 0U &&
         inventory_item_quantity(
             campaign.inventory, program.required_item_id) <
             program.required_item_quantity))
        return false;
    if (program.trigger_kind == CampaignTriggerKind::actor_activation)
        return world.last_actor_activation.occurred &&
               world.last_actor_activation.map_id ==
                   program.trigger_map_id &&
               world.last_actor_activation.actor_index ==
                   program.trigger_x;
    if (program.trigger_kind == CampaignTriggerKind::map_entry)
        return world.last_warp.occurred &&
               world.last_warp.destination_map_id ==
                   program.trigger_map_id &&
               world.player.map_index < world.maps.size() &&
               world.maps[world.player.map_index].id ==
                   program.trigger_map_id;
    if (program.trigger_kind ==
        CampaignTriggerKind::player_rectangle)
        return world.player.map_index < world.maps.size() &&
               world.maps[world.player.map_index].id ==
                   program.trigger_map_id &&
               world.player.x >= program.trigger_x &&
               world.player.y >= program.trigger_y &&
               world.player.x <
                   static_cast<std::int32_t>(program.trigger_x) +
                       program.trigger_width &&
               world.player.y <
                   static_cast<std::int32_t>(program.trigger_y) +
                       program.trigger_height;
    return world.player.map_index < world.maps.size() &&
           world.maps[world.player.map_index].id ==
               program.trigger_map_id &&
           world.player.y == program.trigger_y;
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
    CampaignProgramCatalog loaded;
    if (!input.read(magic.data(), static_cast<std::streamsize>(magic.size())) ||
        magic != std::array{'P', 'C', 'P', '8'}) {
        error = "campaign program cache has an invalid header";
        return false;
    }

    loaded.source = path;
    for (std::string& cell : loaded.naming.uppercase)
        if (!read_string(input, cell)) {
            error = "campaign naming profile is truncated";
            return false;
        }
    for (std::string& cell : loaded.naming.lowercase)
        if (!read_string(input, cell)) {
            error = "campaign naming profile is truncated";
            return false;
        }
    if (!read_string(input, loaded.naming.uppercase_action) ||
        !read_string(input, loaded.naming.lowercase_action) ||
        !read_u8(input, loaded.naming.maximum_length) ||
        !read_string(input, loaded.nickname_heading) ||
        !read_u16(input, loaded.inventory_stack_capacity) ||
        loaded.inventory_stack_capacity == 0U ||
        !valid_naming_profile(loaded.naming)) {
        error = "campaign metadata is invalid";
        return false;
    }
    if (!read_u16(input, program_count) || program_count == 0U ||
        program_count > 4096U) {
        error = "campaign program cache has an invalid program count";
        return false;
    }
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
                    CampaignTriggerKind::player_rectangle) ||
            !read_u8(input, program.trigger_map_id) ||
            !read_u8(input, program.trigger_x) ||
            !read_u8(input, program.trigger_y) ||
            !read_u8(input, program.trigger_width) ||
            !read_u8(input, program.trigger_height) ||
            !read_u32(input, program.required_flag) ||
            !read_u32(input, program.absent_flag) ||
            !read_u16(input, program.required_variable) ||
            !read_u16(input, program.required_variable_value) ||
            !read_u16(input, program.required_item_id) ||
            !read_u16(input, program.required_item_quantity) ||
            !read_u16(input, hidden_count) || hidden_count > 512U)
            return false;
        if (program.required_variable != 0xFFFFU &&
            program.required_variable >= 64U) {
            error =
                "campaign program has an invalid required variable";
            return false;
        }
        if ((program.required_item_id == 0U) !=
            (program.required_item_quantity == 0U)) {
            error =
                "campaign program has an invalid required item";
            return false;
        }
        program.trigger_kind =
            static_cast<CampaignTriggerKind>(trigger_kind);
        if (program.trigger_kind ==
                CampaignTriggerKind::player_rectangle &&
            (program.trigger_width == 0U ||
             program.trigger_height == 0U)) {
            error =
                "campaign rectangle trigger has an empty extent";
            return false;
        }
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
        for (const CampaignInstruction& instruction :
             program.instructions) {
            if ((instruction.opcode == CampaignOpcode::jump ||
                 instruction.opcode ==
                     CampaignOpcode::jump_if_player_y ||
                 instruction.opcode ==
                     CampaignOpcode::jump_if_item_grant_failed) &&
                instruction.value >= program.instructions.size()) {
                error =
                    "campaign program jump leaves its instruction range";
                return false;
            }
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
    if (campaign.inventory.stack_capacity == 0U)
        campaign.inventory.stack_capacity =
            programs.inventory_stack_capacity;

    CampaignFiberState& fiber = campaign.fiber;
    if (!fiber.active) {
        for (std::size_t index = 0U; index < programs.programs.size(); ++index) {
            if (!trigger_ready(programs.programs[index], world, campaign)) continue;
            fiber = {
                .program_index = index,
                .instruction_index = 0U,
                .waiting_ticks = 0U,
                .waiting_dialogue = false,
                .waiting_motion = false,
                .waiting_choice = false,
                .waiting_naming = false,
                .waiting_battle = false,
                .naming_party_index = 0U,
                .last_choice = 0U,
                .last_item_grant_succeeded = false,
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

    if (fiber.waiting_ticks > 0U) {
        --fiber.waiting_ticks;
        if (fiber.waiting_ticks > 0U) {
            error.clear();
            return true;
        }
    }
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
    if (fiber.waiting_naming) {
        if (world.naming.open) {
            error.clear();
            return true;
        }
        if (!world.naming.decided ||
            fiber.naming_party_index >= campaign.party.members.size()) {
            error = "campaign naming screen closed without a result";
            return false;
        }
        if (!world.naming.value.empty())
            campaign.party.members[fiber.naming_party_index].nickname =
                world.naming.value;
        world.naming = {};
        fiber.waiting_naming = false;
    }
    if (fiber.waiting_battle) {
        if (campaign.trainer_battle_request.pending ||
            campaign.battle.active) {
            error.clear();
            return true;
        }
        if (campaign.battle.outcome == BattleOutcome::ongoing) {
            error =
                "campaign trainer battle closed without an outcome";
            return false;
        }
        fiber.waiting_battle = false;
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
        case CampaignOpcode::clear_flag:
            set_campaign_flag(campaign, instruction.value, false);
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
        case CampaignOpcode::nickname_last_party_member_if_yes: {
            if (fiber.last_choice != 0U) break;
            if (campaign.party.members.empty()) {
                error =
                    "campaign cannot nickname an empty party";
                return false;
            }
            PokemonState& pokemon = campaign.party.members.back();
            std::string heading = programs.nickname_heading;
            const std::string token = "{name_buffer}";
            for (std::size_t position = heading.find(token);
                 position != std::string::npos;
                 position = heading.find(token, position + pokemon.nickname.size())) {
                heading.replace(position, token.size(), pokemon.nickname);
            }
            begin_naming(programs.naming, std::move(heading),
                         world.naming);
            fiber.naming_party_index = static_cast<std::uint8_t>(
                campaign.party.members.size() - 1U);
            fiber.waiting_naming = true;
            error.clear();
            return true;
        }
        case CampaignOpcode::player_path:
            if (!start_world_player_motion(
                    world, instruction.player_path, error))
                return false;
            fiber.waiting_motion = true;
            error.clear();
            return true;
        case CampaignOpcode::give_item:
            if (!give_inventory_item(
                    campaign.inventory,
                    static_cast<std::uint16_t>(instruction.value),
                    instruction.a)) {
                error = "campaign could not add an inventory item";
                return false;
            }
            break;
        case CampaignOpcode::try_give_item:
            fiber.last_item_grant_succeeded =
                give_inventory_item(
                    campaign.inventory,
                    static_cast<std::uint16_t>(
                        instruction.value),
                    instruction.a);
            break;
        case CampaignOpcode::take_item:
            if (!take_inventory_item(
                    campaign.inventory,
                    static_cast<std::uint16_t>(instruction.value),
                    instruction.a)) {
                error = "campaign could not remove an inventory item";
                return false;
            }
            break;
        case CampaignOpcode::place_actor: {
            const std::int32_t x = static_cast<std::int16_t>(
                instruction.value & 0xFFFFU);
            const std::int32_t y = static_cast<std::int16_t>(
                instruction.value >> 16U);
            if (!place_world_actor(world, instruction.b,
                                   instruction.a, x, y, error))
                return false;
            break;
        }
        case CampaignOpcode::actor_path: {
            const std::vector<WorldPathCommand> player_waits(
                instruction.actor_path.size(),
                WorldPathCommand::wait);
            if (!start_world_parallel_motion(
                    world, instruction.b, instruction.a,
                    instruction.actor_path, player_waits,
                    instruction.value != 0U, error))
                return false;
            fiber.waiting_motion = true;
            error.clear();
            return true;
        }
        case CampaignOpcode::jump_if_player_y:
            if (world.player.y ==
                static_cast<std::int32_t>(instruction.a))
                fiber.instruction_index = instruction.value;
            break;
        case CampaignOpcode::jump_if_item_grant_failed:
            if (!fiber.last_item_grant_succeeded)
                fiber.instruction_index = instruction.value;
            break;
        case CampaignOpcode::jump:
            fiber.instruction_index = instruction.value;
            break;
        case CampaignOpcode::wait_ticks:
            fiber.waiting_ticks = instruction.value;
            error.clear();
            return true;
        case CampaignOpcode::actor_path_by_player_x: {
            const std::vector<WorldPathCommand>& selected =
                world.player.x == static_cast<std::int32_t>(instruction.b)
                    ? instruction.actor_path
                    : instruction.player_path;
            const std::vector<WorldPathCommand> player_waits(
                selected.size(), WorldPathCommand::wait);
            if (!start_world_parallel_motion(
                    world,
                    static_cast<std::uint8_t>(instruction.value),
                    instruction.a, selected, player_waits, false, error,
                    true, true))
                return false;
            fiber.waiting_motion = true;
            error.clear();
            return true;
        }
        case CampaignOpcode::actor_path_by_player_y: {
            const std::vector<WorldPathCommand>& selected =
                world.player.y == static_cast<std::int32_t>(instruction.b)
                    ? instruction.actor_path
                    : instruction.player_path;
            const std::vector<WorldPathCommand> player_waits(
                selected.size(), WorldPathCommand::wait);
            if (!start_world_parallel_motion(
                    world,
                    static_cast<std::uint8_t>(instruction.value),
                    instruction.a, selected, player_waits, false, error,
                    true, true))
                return false;
            fiber.waiting_motion = true;
            error.clear();
            return true;
        }
        case CampaignOpcode::start_trainer_battle:
            if (campaign.trainer_battle_request.pending ||
                campaign.battle.active) {
                error =
                    "campaign attempted to overlap trainer battles";
                return false;
            }
            campaign.trainer_battle_request = {
                .trainer_class_id = instruction.a,
                .trainer_party_index =
                    static_cast<std::uint16_t>(instruction.value),
                .pending = true,
            };
            fiber.waiting_battle = true;
            error.clear();
            return true;
        case CampaignOpcode::say_if_player_won:
            if (campaign.battle.outcome !=
                BattleOutcome::player_victory)
                break;
            open_program_dialogue(world, campaign, rules, instruction);
            fiber.waiting_dialogue = world.dialogue.open;
            error.clear();
            return true;
        case CampaignOpcode::say_if_player_lost:
            if (campaign.battle.outcome !=
                BattleOutcome::player_defeat)
                break;
            open_program_dialogue(world, campaign, rules, instruction);
            fiber.waiting_dialogue = world.dialogue.open;
            error.clear();
            return true;
        case CampaignOpcode::end_if_player_lost:
            if (campaign.battle.outcome ==
                BattleOutcome::player_defeat) {
                campaign.input_locked = false;
                fiber = {};
                error.clear();
                return true;
            }
            break;
        case CampaignOpcode::heal_party:
            heal_party(campaign.party);
            break;
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
