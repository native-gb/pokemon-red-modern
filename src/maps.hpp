#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pokered {

struct InteractionCatalog;

struct MapTileset {
    std::uint8_t id{};
    std::uint16_t tile_count{};
    std::uint8_t animation_mode{};
    std::vector<std::uint8_t> passable_tiles;
    std::vector<std::uint8_t> pixels;
    std::vector<std::uint8_t> animation_pixels;
};

enum class WorldActorKind : std::uint8_t {
    npc,
    trainer_or_pokemon,
    item,
};

struct WorldSprite {
    std::uint8_t id{};
    std::string key;
    bool still{};
    // Four normalized 16 by 16 standing frames: down, up, left, right.
    std::vector<std::uint8_t> pixels;
};

struct WorldSpace {
    std::uint16_t id{};
    std::string key;
    bool outdoor{};
};

struct WorldWarp {
    std::uint8_t index{};
    std::uint8_t x{};
    std::uint8_t y{};
    std::uint8_t destination_map_id{};
    std::uint8_t destination_warp_index{};
};

struct WorldMovementBounds {
    // Half-open global 16 by 16 world-cell bounds.
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
};

struct WorldActorSpawn {
    std::uint8_t index{};
    std::uint8_t sprite_id{};
    std::uint8_t x{};
    std::uint8_t y{};
    std::uint8_t movement{};
    std::uint8_t direction_or_axis{};
    std::uint8_t text_id{};
    std::uint8_t parameter_a{};
    std::uint8_t parameter_b{};
    WorldActorKind kind{WorldActorKind::npc};
    std::optional<WorldMovementBounds> movement_bounds;
};

struct WorldMap {
    std::uint8_t id{};
    std::uint8_t tileset_id{};
    std::uint8_t width_blocks{};
    std::uint8_t height_blocks{};
    std::uint16_t width_tiles{};
    std::uint16_t height_tiles{};
    std::string key;
    std::string display_name;
    std::int32_t global_x_tiles{};
    std::int32_t global_y_tiles{};
    std::uint16_t world_space{};
    std::vector<std::uint8_t> tiles;
    std::vector<WorldWarp> warps;
    std::vector<WorldActorSpawn> actors;
};

enum class WorldView {
    selected,
    world,
};

enum class WorldDirection : std::uint8_t {
    down,
    up,
    left,
    right,
};

struct WorldActorState {
    std::size_t map_index{};
    std::size_t spawn_index{};
    std::int32_t x{};
    std::int32_t y{};
    float visual_global_x{};
    float visual_global_y{};
    WorldDirection facing{WorldDirection::down};
};

struct WorldPlayerState {
    std::size_t map_index{};
    std::int32_t x{};
    std::int32_t y{};
    float visual_global_x{};
    float visual_global_y{};
    WorldDirection facing{WorldDirection::down};
    std::uint8_t move_cooldown{};
    std::size_t last_outdoor_map_index{};
    bool initialized{};
};

struct WorldWarpState {
    std::uint8_t source_map_id{};
    std::uint8_t source_warp_index{};
    std::uint8_t destination_map_id{};
    std::uint8_t destination_warp_index{};
    std::uint64_t simulation_tick{};
    bool occurred{};
};

struct WorldMapCellIndex {
    std::uint8_t map_id{};
    std::uint16_t width{};
    std::uint16_t height{};
    // -1 means empty. Values are indexes into WorldState::actors.
    std::vector<std::int32_t> actor_by_cell;
    // Zero means no trigger; nonzero values are map-local interaction program IDs.
    std::vector<std::uint8_t> background_program_by_cell;
};

struct DialogueState {
    std::vector<std::string> pages;
    std::size_t page{};
    bool open{};
};

struct WorldStepInput {
    bool left{};
    bool right{};
    bool up{};
    bool down{};
    bool activate{};
};

struct WorldState {
    std::filesystem::path source;
    std::vector<MapTileset> tilesets;
    std::vector<WorldSprite> sprites;
    std::vector<WorldSpace> spaces;
    std::vector<WorldMap> maps;
    std::vector<WorldActorState> actors;
    std::vector<WorldMapCellIndex> spatial;
    std::vector<std::vector<std::size_t>> roam_schedule;
    WorldPlayerState player;
    DialogueState dialogue;
    WorldWarpState last_warp;
    std::size_t current{};
    std::uint16_t current_space{};
    WorldView view{WorldView::world};
    float zoom{1.0F};
    float target_zoom{1.0F};
    float camera_x{};
    float camera_y{};
    float target_camera_x{};
    float target_camera_y{};
    std::uint64_t animation_tick{};
    std::uint64_t simulation_tick{};
    std::uint32_t random_state{0xC001D00DU};
    bool show_annotations{};
    bool follow_player{true};
    bool camera_initialized{};
    bool loaded{};
};

bool load_world(const std::filesystem::path& path, WorldState& result, std::string& error);
bool initialize_world_runtime(WorldState& world, const InteractionCatalog& interactions,
                              std::string& error);
bool enter_world_at(WorldState& world, std::uint8_t map_id, std::int32_t x,
                    std::int32_t y, std::string& error);
void step_world(WorldState& world, const InteractionCatalog& interactions,
                const WorldStepInput& input);
void select_next_map(WorldState& world);
void select_previous_map(WorldState& world);
void toggle_world_view(WorldState& world);
void zoom_world_view(WorldState& world, float factor);
void pan_world_view(WorldState& world, float x, float y);
void reset_world_view(WorldState& world);
void update_world_view(WorldState& world, double elapsed_seconds);
void step_world_animation(WorldState& world);
const WorldMap* selected_map(const WorldState& world);
const WorldSpace* current_world_space(const WorldState& world);
const MapTileset* find_tileset(const WorldState& world, std::uint8_t id);
const WorldSprite* find_world_sprite(const WorldState& world, std::uint8_t id);
const WorldMapCellIndex* find_spatial_index(const WorldState& world, std::uint8_t map_id);
bool actor_can_roam_to(const WorldActorSpawn& actor, std::int32_t global_x, std::int32_t global_y);
std::string_view selected_map_name(const WorldState& world);
std::string_view label(WorldView view);
std::string_view label(WorldActorKind kind);

} // namespace pokered
