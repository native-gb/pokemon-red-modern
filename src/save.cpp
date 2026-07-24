#include "save.hpp"

#include "maps.hpp"
#include "sexpr.hpp"
#include "state.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace pokered {
namespace {

constexpr std::int64_t kSaveSchema = 1;

std::string quote_string(std::string_view value) {
    std::string result{"\""};
    for (char character : value) {
        switch (character) {
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        default:
            result.push_back(character);
            break;
        }
    }
    result.push_back('"');
    return result;
}

std::string_view direction_name(WorldDirection direction) {
    switch (direction) {
    case WorldDirection::down:
        return "down";
    case WorldDirection::up:
        return "up";
    case WorldDirection::left:
        return "left";
    case WorldDirection::right:
        return "right";
    }
    return "down";
}

bool parse_direction(const sexpr::Atom& atom,
                     WorldDirection& result) {
    if (atom.kind != sexpr::AtomKind::symbol) return false;
    if (atom.symbol.text == "down")
        result = WorldDirection::down;
    else if (atom.symbol.text == "up")
        result = WorldDirection::up;
    else if (atom.symbol.text == "left")
        result = WorldDirection::left;
    else if (atom.symbol.text == "right")
        result = WorldDirection::right;
    else
        return false;
    return true;
}

const sexpr::Form* child(const sexpr::Form& form,
                         std::string_view head) {
    const auto found = std::ranges::find_if(
        form.children, [&](const sexpr::Form& candidate) {
            return sexpr::is_head(candidate, head);
        });
    return found == form.children.end() ? nullptr : &*found;
}

bool integer(const sexpr::Form& form, std::size_t index,
             std::int64_t minimum, std::int64_t maximum,
             std::int64_t& result) {
    const sexpr::Atom* atom = sexpr::argument(form, index);
    if (atom == nullptr || atom->kind != sexpr::AtomKind::integer ||
        atom->integer < minimum || atom->integer > maximum)
        return false;
    result = atom->integer;
    return true;
}

bool string_value(const sexpr::Form& form, std::size_t index,
                  std::string& result) {
    const sexpr::Atom* atom = sexpr::argument(form, index);
    if (atom == nullptr || atom->kind != sexpr::AtomKind::string)
        return false;
    result = atom->string;
    return true;
}

bool boolean(const sexpr::Form& form, std::size_t index,
             bool& result) {
    const sexpr::Atom* atom = sexpr::argument(form, index);
    if (atom == nullptr || atom->kind != sexpr::AtomKind::boolean)
        return false;
    result = atom->boolean;
    return true;
}

void write_pokemon(std::ofstream& output,
                   const PokemonState& pokemon,
                   std::string_view indentation) {
    output << indentation << "pokemon\n"
           << indentation << "    species "
           << static_cast<unsigned>(pokemon.species_dex) << '\n'
           << indentation << "    level "
           << static_cast<unsigned>(pokemon.level) << '\n'
           << indentation << "    experience "
           << pokemon.experience << '\n'
           << indentation << "    dvs";
    for (std::uint8_t value : pokemon.dvs)
        output << ' ' << static_cast<unsigned>(value);
    output << '\n' << indentation << "    stat_experience";
    for (std::uint16_t value : pokemon.stat_experience)
        output << ' ' << value;
    output << '\n'
           << indentation << "    stats " << pokemon.stats.hp
           << ' ' << pokemon.stats.attack << ' '
           << pokemon.stats.defense << ' ' << pokemon.stats.speed
           << ' ' << pokemon.stats.special << '\n'
           << indentation << "    current_hp "
           << pokemon.current_hp << '\n'
           << indentation << "    status "
           << static_cast<unsigned>(pokemon.status) << '\n'
           << indentation << "    sleep_turns "
           << static_cast<unsigned>(pokemon.sleep_turns) << '\n'
           << indentation << "    trainer_id "
           << pokemon.trainer_id << '\n'
           << indentation << "    original_trainer "
           << quote_string(pokemon.original_trainer) << '\n'
           << indentation << "    nickname "
           << quote_string(pokemon.nickname) << '\n';
    for (const PokemonMoveState& move : pokemon.moves)
        output << indentation << "    move "
               << static_cast<unsigned>(move.move_id) << ' '
               << static_cast<unsigned>(move.pp) << ' '
               << static_cast<unsigned>(move.maximum_pp) << ' '
               << static_cast<unsigned>(move.pp_ups) << '\n';
}

bool parse_pokemon(const sexpr::Form& form,
                   PokemonState& result) {
    if (!sexpr::is_head(form, "pokemon")) return false;
    PokemonState pokemon;
    std::int64_t value = 0;
    const sexpr::Form* species = child(form, "species");
    const sexpr::Form* level = child(form, "level");
    const sexpr::Form* experience = child(form, "experience");
    const sexpr::Form* dvs = child(form, "dvs");
    const sexpr::Form* stat_experience =
        child(form, "stat_experience");
    const sexpr::Form* stats = child(form, "stats");
    const sexpr::Form* current_hp = child(form, "current_hp");
    const sexpr::Form* status = child(form, "status");
    const sexpr::Form* sleep_turns = child(form, "sleep_turns");
    const sexpr::Form* trainer_id = child(form, "trainer_id");
    const sexpr::Form* original_trainer =
        child(form, "original_trainer");
    const sexpr::Form* nickname = child(form, "nickname");
    if (species == nullptr || level == nullptr ||
        experience == nullptr || dvs == nullptr ||
        stat_experience == nullptr || stats == nullptr ||
        current_hp == nullptr || status == nullptr ||
        sleep_turns == nullptr || trainer_id == nullptr ||
        original_trainer == nullptr || nickname == nullptr)
        return false;
    if (!integer(*species, 0U, 1, 255, value)) return false;
    pokemon.species_dex = static_cast<std::uint8_t>(value);
    if (!integer(*level, 0U, 1, 100, value)) return false;
    pokemon.level = static_cast<std::uint8_t>(value);
    if (!integer(*experience, 0U, 0,
                 std::numeric_limits<std::uint32_t>::max(), value))
        return false;
    pokemon.experience = static_cast<std::uint32_t>(value);
    if (dvs->arguments.size() != pokemon.dvs.size() ||
        stat_experience->arguments.size() !=
            pokemon.stat_experience.size() ||
        stats->arguments.size() != 5U)
        return false;
    for (std::size_t index = 0U; index < pokemon.dvs.size();
         ++index) {
        if (!integer(*dvs, index, 0, 15, value)) return false;
        pokemon.dvs[index] = static_cast<std::uint8_t>(value);
    }
    for (std::size_t index = 0U;
         index < pokemon.stat_experience.size(); ++index) {
        if (!integer(*stat_experience, index, 0, 65535, value))
            return false;
        pokemon.stat_experience[index] =
            static_cast<std::uint16_t>(value);
    }
    std::array<std::uint16_t*, 5> stats_out{
        &pokemon.stats.hp, &pokemon.stats.attack,
        &pokemon.stats.defense, &pokemon.stats.speed,
        &pokemon.stats.special};
    for (std::size_t index = 0U; index < stats_out.size();
         ++index) {
        if (!integer(*stats, index, 1, 65535, value))
            return false;
        *stats_out[index] = static_cast<std::uint16_t>(value);
    }
    if (!integer(*current_hp, 0U, 0, pokemon.stats.hp, value))
        return false;
    pokemon.current_hp = static_cast<std::uint16_t>(value);
    if (!integer(*status, 0U, 0,
                 static_cast<std::int64_t>(
                     MajorStatus::paralysis), value))
        return false;
    pokemon.status = static_cast<MajorStatus>(value);
    if (!integer(*sleep_turns, 0U, 0, 255, value))
        return false;
    pokemon.sleep_turns = static_cast<std::uint8_t>(value);
    if (!integer(*trainer_id, 0U, 0, 65535, value))
        return false;
    pokemon.trainer_id = static_cast<std::uint16_t>(value);
    if (!string_value(*original_trainer, 0U,
                      pokemon.original_trainer) ||
        !string_value(*nickname, 0U, pokemon.nickname))
        return false;

    std::size_t move_index = 0U;
    for (const sexpr::Form& candidate : form.children) {
        if (!sexpr::is_head(candidate, "move")) continue;
        if (move_index >= pokemon.moves.size() ||
            candidate.arguments.size() != 4U)
            return false;
        PokemonMoveState& move = pokemon.moves[move_index++];
        if (!integer(candidate, 0U, 0, 255, value))
            return false;
        move.move_id = static_cast<std::uint8_t>(value);
        if (!integer(candidate, 1U, 0, 255, value))
            return false;
        move.pp = static_cast<std::uint8_t>(value);
        if (!integer(candidate, 2U, 0, 255, value))
            return false;
        move.maximum_pp = static_cast<std::uint8_t>(value);
        if (!integer(candidate, 3U, 0, 3, value))
            return false;
        move.pp_ups = static_cast<std::uint8_t>(value);
    }
    if (move_index != pokemon.moves.size()) return false;
    result = std::move(pokemon);
    return true;
}

std::optional<std::size_t> map_index(
    const WorldState& world, std::uint8_t id) {
    const auto found = std::ranges::find_if(
        world.maps, [id](const WorldMap& map) {
            return map.id == id;
        });
    if (found == world.maps.end()) return std::nullopt;
    return static_cast<std::size_t>(found - world.maps.begin());
}

} // namespace

bool save_game_exists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

bool save_game(const std::filesystem::path& path,
               const CampaignState& campaign,
               const WorldState& world, std::string& error) {
    if (!campaign.initialized || !world.loaded ||
        !world.player.initialized ||
        world.player.map_index >= world.maps.size() ||
        world.player.last_outdoor_map_index >= world.maps.size()) {
        error = "campaign is not in a saveable world state";
        return false;
    }
    if (campaign.battle.active || campaign.fiber.active ||
        campaign.input_locked) {
        error = "finish the current event before saving";
        return false;
    }

    std::error_code filesystem_error;
    std::filesystem::create_directories(
        path.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        return false;
    }
    const std::filesystem::path temporary =
        path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) {
        error = "could not create campaign save";
        return false;
    }

    const WorldMap& player_map =
        world.maps[world.player.map_index];
    const WorldMap& outdoor_map =
        world.maps[world.player.last_outdoor_map_index];
    output << "pokemon_red_modern_save " << kSaveSchema << '\n'
           << "    player_name " << quote_string(campaign.player_name)
           << '\n'
           << "    rival_name " << quote_string(campaign.rival_name)
           << '\n'
           << "    options "
           << static_cast<unsigned>(campaign.options[0]) << ' '
           << static_cast<unsigned>(campaign.options[1]) << ' '
           << static_cast<unsigned>(campaign.options[2]) << '\n'
           << "    trainer_id " << campaign.trainer_id << '\n'
           << "    money " << campaign.money << '\n'
           << "    play_steps " << campaign.play_steps << '\n'
           << "    healing "
           << static_cast<unsigned>(campaign.last_healing_map_id)
           << ' ' << static_cast<unsigned>(campaign.last_healing_x)
           << ' ' << static_cast<unsigned>(campaign.last_healing_y)
           << ' ' << (campaign.used_pokemon_center ? "true" : "false")
           << ' ' << (campaign.has_healing_checkpoint ? "true" : "false")
           << '\n'
           << "    flags";
    for (std::uint8_t flag : campaign.flags)
        output << ' ' << static_cast<unsigned>(flag != 0U);
    output << '\n' << "    variables";
    for (std::uint16_t variable : campaign.variables)
        output << ' ' << variable;
    output << '\n'
           << "    random_state " << world.random_state << '\n'
           << "    player " << static_cast<unsigned>(player_map.id)
           << ' ' << world.player.x << ' ' << world.player.y << ' '
           << direction_name(world.player.facing) << ' '
           << static_cast<unsigned>(outdoor_map.id) << '\n'
           << "    inventory " << campaign.inventory.stack_capacity
           << '\n';
    for (const InventoryStack& stack : campaign.inventory.stacks)
        output << "        stack " << stack.item_id << ' '
               << stack.quantity << '\n';
    output << "    party\n";
    for (const PokemonState& pokemon : campaign.party.members)
        write_pokemon(output, pokemon, "        ");
    output << "    storage " << campaign.storage.box_capacity
           << ' '
           << static_cast<unsigned>(campaign.storage.current_box)
           << '\n';
    for (std::size_t index = 0U;
         index < campaign.storage.boxes.size(); ++index) {
        output << "        box " << index << '\n';
        for (const PokemonState& pokemon :
             campaign.storage.boxes[index])
            write_pokemon(output, pokemon, "            ");
    }
    output << "    daycare "
           << (campaign.daycare.occupied ? "true" : "false")
           << ' '
           << static_cast<unsigned>(
                  campaign.daycare.deposited_level)
           << '\n';
    if (campaign.daycare.occupied)
        write_pokemon(output, campaign.daycare.pokemon, "        ");
    output << "    actors\n";
    for (const WorldActorState& actor : world.actors) {
        if (actor.map_index >= world.maps.size()) continue;
        output << "        actor "
               << static_cast<unsigned>(
                      world.maps[actor.map_index].id)
               << ' ' << actor.spawn_index << ' ' << actor.x << ' '
               << actor.y << ' ' << direction_name(actor.facing)
               << ' ' << (actor.visible ? "true" : "false") << '\n';
    }
    output.close();
    if (!output) {
        error = "could not finish campaign save";
        std::filesystem::remove(temporary, filesystem_error);
        return false;
    }
    std::filesystem::rename(temporary, path, filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        std::filesystem::remove(temporary, filesystem_error);
        return false;
    }
    error.clear();
    return true;
}

bool load_game(const std::filesystem::path& path,
               CampaignState& campaign, WorldState& world,
               const InteractionCatalog& interactions,
               std::string& error) {
    const auto invalid_save = [&error]() {
        error = "campaign save contains invalid state";
        return false;
    };
    std::ifstream input(path);
    if (!input) {
        error = "campaign save does not exist";
        return false;
    }
    const std::string source{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    sexpr::Document document;
    Diagnostics diagnostics;
    if (!sexpr::parse(path.string(), source, document, diagnostics) ||
        document.forms.size() != 1U ||
        !sexpr::is_head(
            document.forms.front(),
            "pokemon_red_modern_save")) {
        error = diagnostics.entries.empty()
                    ? "campaign save has an invalid root"
                    : format_diagnostic(diagnostics.entries.front());
        return false;
    }
    const sexpr::Form& root = document.forms.front();
    std::int64_t value = 0;
    if (!integer(root, 0U, kSaveSchema, kSaveSchema, value)) {
        error = "campaign save has an unsupported schema";
        return false;
    }

    const sexpr::Form* player_name = child(root, "player_name");
    const sexpr::Form* rival_name = child(root, "rival_name");
    const sexpr::Form* options = child(root, "options");
    const sexpr::Form* trainer_id = child(root, "trainer_id");
    const sexpr::Form* money = child(root, "money");
    const sexpr::Form* play_steps = child(root, "play_steps");
    const sexpr::Form* healing = child(root, "healing");
    const sexpr::Form* flags = child(root, "flags");
    const sexpr::Form* variables = child(root, "variables");
    const sexpr::Form* random_state = child(root, "random_state");
    const sexpr::Form* player = child(root, "player");
    const sexpr::Form* inventory = child(root, "inventory");
    const sexpr::Form* party = child(root, "party");
    const sexpr::Form* storage = child(root, "storage");
    const sexpr::Form* daycare = child(root, "daycare");
    const sexpr::Form* actors = child(root, "actors");
    if (player_name == nullptr || rival_name == nullptr ||
        options == nullptr || trainer_id == nullptr ||
        money == nullptr || play_steps == nullptr ||
        healing == nullptr || flags == nullptr ||
        variables == nullptr || random_state == nullptr ||
        player == nullptr || inventory == nullptr ||
        party == nullptr || storage == nullptr ||
        daycare == nullptr || actors == nullptr) {
        error = "campaign save is missing required state";
        return false;
    }

    CampaignState loaded;
    if (!string_value(*player_name, 0U, loaded.player_name) ||
        loaded.player_name.empty() ||
        !string_value(*rival_name, 0U, loaded.rival_name) ||
        loaded.rival_name.empty() ||
        options->arguments.size() != loaded.options.size())
        return invalid_save();
    for (std::size_t index = 0U; index < loaded.options.size();
         ++index) {
        if (!integer(*options, index, 0, 2, value)) return invalid_save();
        loaded.options[index] = static_cast<std::uint8_t>(value);
    }
    if (!integer(*trainer_id, 0U, 0, 65535, value))
        return invalid_save();
    loaded.trainer_id = static_cast<std::uint16_t>(value);
    if (!integer(*money, 0U, 0,
                 std::numeric_limits<std::uint32_t>::max(), value))
        return invalid_save();
    loaded.money = static_cast<std::uint32_t>(value);
    if (!integer(*play_steps, 0U, 0,
                 std::numeric_limits<std::int64_t>::max(), value))
        return invalid_save();
    loaded.play_steps = static_cast<std::uint64_t>(value);
    if (healing->arguments.size() != 5U ||
        !integer(*healing, 0U, 0, 255, value))
        return invalid_save();
    loaded.last_healing_map_id =
        static_cast<std::uint8_t>(value);
    if (!integer(*healing, 1U, 0, 255, value)) return invalid_save();
    loaded.last_healing_x = static_cast<std::uint8_t>(value);
    if (!integer(*healing, 2U, 0, 255, value)) return invalid_save();
    loaded.last_healing_y = static_cast<std::uint8_t>(value);
    if (!boolean(*healing, 3U, loaded.used_pokemon_center) ||
        !boolean(*healing, 4U,
                 loaded.has_healing_checkpoint))
        return invalid_save();
    loaded.flags.reserve(flags->arguments.size());
    for (std::size_t index = 0U; index < flags->arguments.size();
         ++index) {
        if (!integer(*flags, index, 0, 1, value)) return invalid_save();
        loaded.flags.push_back(static_cast<std::uint8_t>(value));
    }
    loaded.variables.reserve(variables->arguments.size());
    for (std::size_t index = 0U;
         index < variables->arguments.size(); ++index) {
        if (!integer(*variables, index, 0, 65535, value))
            return invalid_save();
        loaded.variables.push_back(
            static_cast<std::uint16_t>(value));
    }
    if (!integer(*inventory, 0U, 1, 65535, value))
        return invalid_save();
    loaded.inventory.stack_capacity =
        static_cast<std::uint16_t>(value);
    for (const sexpr::Form& stack : inventory->children) {
        if (!sexpr::is_head(stack, "stack") ||
            stack.arguments.size() != 2U ||
            !integer(stack, 0U, 1, 65535, value))
            return invalid_save();
        InventoryStack parsed{
            .item_id = static_cast<std::uint16_t>(value)};
        if (!integer(stack, 1U, 1, 65535, value))
            return invalid_save();
        parsed.quantity = static_cast<std::uint16_t>(value);
        loaded.inventory.stacks.push_back(parsed);
    }
    if (loaded.inventory.stacks.size() >
        loaded.inventory.stack_capacity)
        return invalid_save();
    for (const sexpr::Form& pokemon : party->children) {
        PokemonState parsed;
        if (!parse_pokemon(pokemon, parsed)) return invalid_save();
        loaded.party.members.push_back(std::move(parsed));
    }
    if (loaded.party.members.size() > 6U) return invalid_save();
    if (!integer(*storage, 0U, 1, 65535, value))
        return invalid_save();
    loaded.storage.box_capacity =
        static_cast<std::uint16_t>(value);
    if (!integer(*storage, 1U, 0, 255, value)) return invalid_save();
    loaded.storage.current_box = static_cast<std::uint8_t>(value);
    for (const sexpr::Form& box : storage->children) {
        if (!sexpr::is_head(box, "box") ||
            !integer(box, 0U, 0, 255, value) ||
            static_cast<std::size_t>(value) !=
                loaded.storage.boxes.size())
            return invalid_save();
        loaded.storage.boxes.emplace_back();
        for (const sexpr::Form& pokemon : box.children) {
            PokemonState parsed;
            if (!parse_pokemon(pokemon, parsed)) return invalid_save();
            loaded.storage.boxes.back().push_back(
                std::move(parsed));
        }
        if (loaded.storage.boxes.back().size() >
            loaded.storage.box_capacity)
            return invalid_save();
    }
    if (loaded.storage.boxes.empty() ||
        loaded.storage.current_box >=
            loaded.storage.boxes.size())
        return invalid_save();
    if (!boolean(*daycare, 0U, loaded.daycare.occupied) ||
        !integer(*daycare, 1U, 0, 100, value))
        return invalid_save();
    loaded.daycare.deposited_level =
        static_cast<std::uint8_t>(value);
    if (loaded.daycare.occupied) {
        if (daycare->children.size() != 1U ||
            !parse_pokemon(
                daycare->children.front(),
                loaded.daycare.pokemon))
            return invalid_save();
    } else if (!daycare->children.empty()) {
        return invalid_save();
    }

    if (!integer(*random_state, 0U, 0,
                 std::numeric_limits<std::uint32_t>::max(), value))
        return invalid_save();
    const std::uint32_t loaded_random_state =
        static_cast<std::uint32_t>(value);
    std::int64_t map_id_value = 0;
    std::int64_t x_value = 0;
    std::int64_t y_value = 0;
    std::int64_t outdoor_id_value = 0;
    WorldDirection facing;
    const sexpr::Atom* facing_atom = sexpr::argument(*player, 3U);
    if (player->arguments.size() != 5U ||
        !integer(*player, 0U, 0, 255, map_id_value) ||
        !integer(*player, 1U,
                 std::numeric_limits<std::int32_t>::min(),
                 std::numeric_limits<std::int32_t>::max(), x_value) ||
        !integer(*player, 2U,
                 std::numeric_limits<std::int32_t>::min(),
                 std::numeric_limits<std::int32_t>::max(), y_value) ||
        facing_atom == nullptr ||
        !parse_direction(*facing_atom, facing) ||
        !integer(*player, 4U, 0, 255, outdoor_id_value))
        return invalid_save();
    const auto loaded_map_index =
        map_index(world, static_cast<std::uint8_t>(map_id_value));
    const auto loaded_outdoor_index =
        map_index(
            world,
            static_cast<std::uint8_t>(outdoor_id_value));
    if (!loaded_map_index || !loaded_outdoor_index)
        return invalid_save();

    struct ActorRecord {
        std::size_t map_index{};
        std::size_t spawn_index{};
        std::int32_t x{};
        std::int32_t y{};
        WorldDirection facing{WorldDirection::down};
        bool visible{};
    };
    std::vector<ActorRecord> actor_records;
    actor_records.reserve(actors->children.size());
    for (const sexpr::Form& actor : actors->children) {
        std::int64_t actor_map_id = 0;
        std::int64_t spawn_index = 0;
        std::int64_t actor_x = 0;
        std::int64_t actor_y = 0;
        const sexpr::Atom* actor_facing =
            sexpr::argument(actor, 4U);
        bool visible = false;
        if (!sexpr::is_head(actor, "actor") ||
            actor.arguments.size() != 6U ||
            !integer(actor, 0U, 0, 255, actor_map_id) ||
            !integer(actor, 1U, 0,
                     std::numeric_limits<std::int64_t>::max(),
                     spawn_index) ||
            !integer(actor, 2U,
                     std::numeric_limits<std::int32_t>::min(),
                     std::numeric_limits<std::int32_t>::max(),
                     actor_x) ||
            !integer(actor, 3U,
                     std::numeric_limits<std::int32_t>::min(),
                     std::numeric_limits<std::int32_t>::max(),
                     actor_y) ||
            actor_facing == nullptr ||
            !parse_direction(*actor_facing, facing) ||
            !boolean(actor, 5U, visible))
            return invalid_save();
        const auto actor_map_index =
            map_index(
                world,
                static_cast<std::uint8_t>(actor_map_id));
        if (!actor_map_index ||
            spawn_index >= static_cast<std::int64_t>(
                world.maps[*actor_map_index].actors.size()))
            return invalid_save();
        actor_records.push_back({
            .map_index = *actor_map_index,
            .spawn_index = static_cast<std::size_t>(spawn_index),
            .x = static_cast<std::int32_t>(actor_x),
            .y = static_cast<std::int32_t>(actor_y),
            .facing = facing,
            .visible = visible,
        });
    }
    if (actor_records.size() != world.actors.size())
        return invalid_save();
    for (std::size_t index = 0U;
         index < actor_records.size(); ++index) {
        if (world.actors[index].map_index !=
                actor_records[index].map_index ||
            world.actors[index].spawn_index !=
                actor_records[index].spawn_index)
            return invalid_save();
    }

    loaded.imported_initial_state = true;
    loaded.initialized = true;
    if (!enter_world_at(
            world, static_cast<std::uint8_t>(map_id_value),
            static_cast<std::int32_t>(x_value),
            static_cast<std::int32_t>(y_value), error,
            static_cast<std::uint8_t>(outdoor_id_value)))
        return false;
    world.player.facing = facing;
    world.player.last_outdoor_map_index = *loaded_outdoor_index;
    world.random_state = loaded_random_state;
    for (std::size_t index = 0U;
         index < actor_records.size(); ++index) {
        WorldActorState& actor = world.actors[index];
        const ActorRecord& record = actor_records[index];
        actor.x = record.x;
        actor.y = record.y;
        actor.facing = record.facing;
        actor.visible = record.visible;
        actor.moving = false;
        actor.animation_phase = 0U;
        const WorldMap& map = world.maps[actor.map_index];
        actor.visual_global_x = static_cast<float>(
            map.global_x_tiles / 2 + actor.x);
        actor.visual_global_y = static_cast<float>(
            map.global_y_tiles / 2 + actor.y);
        actor.movement_from_x = actor.visual_global_x;
        actor.movement_from_y = actor.visual_global_y;
        actor.movement_to_x = actor.visual_global_x;
        actor.movement_to_y = actor.visual_global_y;
        actor.movement_elapsed = 0.0F;
    }
    if (!rebuild_world_actor_spatial(
            world, interactions, error))
        return false;
    campaign = std::move(loaded);
    error.clear();
    return true;
}

} // namespace pokered
