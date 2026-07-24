#pragma once

#include "field_menu.hpp"
#include "naming.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pokered {

struct InteractionCatalog;
struct CampaignState;

struct MapTileset {
    std::uint8_t id{};
    std::uint16_t tile_count{};
    std::uint8_t grass_tile{0xFFU};
    std::uint8_t animation_mode{};
    std::array<std::uint8_t, 3> counter_tiles{};
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
    // Sixteen normalized 16 by 16 frames: four walking phases for each
    // down/up/left/right facing.
    std::vector<std::uint8_t> pixels;
};

enum class WorldCameraFraming : std::uint8_t {
    fixed_zoom,
    fit_map,
    fit_width,
    fit_height,
    fit_space,
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
    WorldCameraFraming camera_framing{
        WorldCameraFraming::fixed_zoom};
    float camera_zoom{2.0F};
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

struct WorldLedge {
    WorldDirection direction{WorldDirection::down};
    std::uint8_t standing_tile{};
    std::uint8_t ledge_tile{};
    std::uint8_t required_input_mask{};
};

struct WorldMovementSegment {
    float target_x{};
    float target_y{};
    bool ledge_hop{};
};

enum class WorldPathCommand : std::uint8_t {
    down,
    up,
    left,
    right,
    wait,
    face_down,
};

struct WorldActorState {
    std::size_t map_index{};
    std::size_t spawn_index{};
    std::int32_t x{};
    std::int32_t y{};
    float visual_global_x{};
    float visual_global_y{};
    float movement_from_x{};
    float movement_from_y{};
    float movement_to_x{};
    float movement_to_y{};
    float movement_elapsed{};
    WorldDirection facing{WorldDirection::down};
    std::uint8_t animation_phase{};
    bool moving{};
    bool visible{true};
};

struct WorldPlayerState {
    std::size_t map_index{};
    std::int32_t x{};
    std::int32_t y{};
    float visual_global_x{};
    float visual_global_y{};
    float movement_from_x{};
    float movement_from_y{};
    float movement_to_x{};
    float movement_to_y{};
    float movement_elapsed{};
    float visual_offset_y_pixels{};
    std::deque<WorldMovementSegment> movement_queue;
    WorldDirection facing{WorldDirection::down};
    std::uint8_t move_cooldown{};
    std::uint8_t animation_phase{};
    std::size_t last_outdoor_map_index{};
    bool moving{};
    bool ledge_hop{};
    bool alternate_foot{};
    bool warp_pending{};
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

struct WorldAreaBanner {
    std::string text;
    float elapsed{};
    bool active{};
};

struct WorldMapCellIndex {
    std::uint8_t map_id{};
    std::uint16_t width{};
    std::uint16_t height{};
    // -1 means empty. Values are indexes into WorldState::actors.
    std::vector<std::int32_t> actor_by_cell;
    // Zero means no trigger; nonzero values are map-local interaction program IDs.
    std::vector<std::uint8_t> background_program_by_cell;
    // Runtime actor indexes whose imported sight ray includes each cell.
    std::vector<std::vector<std::size_t>> trainer_sight_actors_by_cell;
};

struct DialogueState {
    std::vector<std::string> pages;
    std::size_t page{};
    bool open{};
};

struct WorldChoiceState {
    std::vector<std::string> options;
    std::size_t selected{};
    std::uint8_t input_cooldown{};
    bool open{};
    bool decided{};
};

struct WorldPokemonPresentation {
    std::uint32_t serial{};
    std::uint16_t species_dex{};
    std::uint8_t page{};
    bool active{};
};

enum class WorldAudioCue : std::uint8_t {
    none,
    get_key_item,
};

struct WorldAudioEvent {
    std::uint32_t serial{};
    WorldAudioCue cue{WorldAudioCue::none};
};

enum class WorldMusicScene : std::uint8_t {
    title,
    meet_prof_oak,
    intro_battle,
};

struct WorldMusicOverride {
    WorldMusicScene scene{WorldMusicScene::title};
    bool active{};
};

struct WorldActorActivation {
    std::uint8_t map_id{};
    std::uint8_t actor_index{};
    bool occurred{};
};

struct WorldCellActivation {
    std::uint8_t map_id{};
    std::uint8_t x{};
    std::uint8_t y{};
    WorldDirection facing{WorldDirection::down};
    bool occurred{};
};

struct WorldOpponentRequest {
    std::uint8_t map_id{};
    std::uint8_t actor_index{};
    bool pending{};
};

struct WorldTrainerApproach {
    std::size_t actor_runtime_index{};
    std::uint8_t steps_remaining{};
    std::uint8_t step_cooldown{};
    WorldDirection direction{WorldDirection::down};
    bool active{};
};

enum class WorldServiceKind : std::uint8_t {
    none,
    pokecenter_nurse,
};

struct WorldServiceState {
    WorldServiceKind kind{WorldServiceKind::none};
    std::vector<std::string> pages;
    bool active{};
};

struct WorldScriptMotion {
    std::size_t actor_runtime_index{};
    std::vector<WorldPathCommand> actor_path;
    std::vector<WorldPathCommand> player_path;
    std::size_t actor_cursor{};
    std::size_t player_cursor{};
    std::uint8_t step_cooldown{};
    bool hide_actor_at_end{};
    bool actor_may_overlap_player{};
    bool actor_ignores_terrain{};
    bool active{};
};

struct WorldStepInput {
    bool left{};
    bool right{};
    bool up{};
    bool down{};
    bool activate{};
    bool erase{};
    bool submit{};
    bool toggle_case{};
    bool start{};
    bool back{};
    const char* text{};
};

struct WorldState {
    std::filesystem::path source;
    std::vector<MapTileset> tilesets;
    std::vector<WorldSprite> sprites;
    std::vector<WorldSpace> spaces;
    std::vector<WorldLedge> ledges;
    std::vector<WorldMap> maps;
    std::vector<WorldActorState> actors;
    std::vector<WorldMapCellIndex> spatial;
    std::vector<std::vector<std::size_t>> roam_schedule;
    WorldPlayerState player;
    DialogueState dialogue;
    WorldChoiceState choice;
    WorldPokemonPresentation pokemon_presentation;
    WorldAudioEvent audio_event;
    WorldMusicOverride music_override;
    NamingState naming;
    FieldMenuState menu;
    WorldWarpState last_warp;
    WorldAreaBanner area_banner;
    WorldActorActivation last_actor_activation;
    WorldCellActivation last_cell_activation;
    WorldOpponentRequest opponent_request;
    WorldTrainerApproach trainer_approach;
    WorldServiceState service;
    WorldScriptMotion script_motion;
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
    std::uint64_t ledge_hop_count{};
    std::uint32_t random_state{0xC001D00DU};
    bool player_completed_step{};
    bool show_annotations{};
    bool follow_player_x{true};
    bool follow_player_y{true};
    bool automatic_camera_framing{true};
    bool manual_camera_override{};
    bool manual_pan_override{};
    bool camera_region_dirty{true};
    std::size_t camera_region_map_index{
        static_cast<std::size_t>(-1)};
    bool camera_initialized{};
    bool loaded{};
};

bool load_world(const std::filesystem::path& path, WorldState& result, std::string& error);
bool initialize_world_runtime(WorldState& world, const InteractionCatalog& interactions,
                              std::string& error);
bool rebuild_world_actor_spatial(
    WorldState& world, const InteractionCatalog& interactions,
    std::string& error);
bool enter_world_at(WorldState& world, std::uint8_t map_id, std::int32_t x,
                    std::int32_t y, std::string& error,
                    std::optional<std::uint8_t> previous_map_id = std::nullopt);
void step_world(WorldState& world, const InteractionCatalog& interactions,
                CampaignState& campaign,
                const WorldStepInput& input);
void select_next_map(WorldState& world);
void select_previous_map(WorldState& world);
void toggle_world_view(WorldState& world);
void zoom_world_view(WorldState& world, float factor);
void pan_world_view(WorldState& world, float x, float y);
void reset_world_view(WorldState& world);
void update_world_camera_region(WorldState& world, int output_width,
                                int output_height);
void update_world_view(WorldState& world, double elapsed_seconds);
void update_world_presentation(WorldState& world,
                               double elapsed_seconds);
void step_world_animation(WorldState& world);
void open_world_dialogue(WorldState& world,
                         const CampaignState& campaign,
                         const std::vector<std::string>& pages);
bool set_world_actor_visible(WorldState& world, std::uint8_t map_id,
                             std::uint8_t actor_index, bool visible,
                             std::string& error);
bool place_world_actor(WorldState& world, std::uint8_t map_id,
                       std::uint8_t actor_index, std::int32_t x,
                       std::int32_t y, std::string& error,
                       bool ignores_terrain = false);
bool face_world_actor(WorldState& world, std::uint8_t map_id,
                      std::uint8_t actor_index, WorldDirection direction,
                      std::string& error);
bool start_world_actor_to_player_motion(
    WorldState& world, std::uint8_t map_id, std::uint8_t actor_index,
    std::int8_t target_y_offset, std::string& error);
bool start_world_pair_alignment(WorldState& world,
                                std::uint8_t map_id,
                                std::uint8_t actor_index,
                                std::uint8_t target_x,
                                std::string& error);
bool start_world_escort_motion(
    WorldState& world, std::uint8_t map_id, std::uint8_t actor_index,
    std::int32_t player_target_x, std::int32_t player_target_y,
    WorldDirection actor_target_side, std::string& error);
bool start_world_parallel_motion(
    WorldState& world, std::uint8_t map_id, std::uint8_t actor_index,
    const std::vector<WorldPathCommand>& actor_path,
    const std::vector<WorldPathCommand>& player_path,
    bool hide_actor_at_end, std::string& error,
    bool actor_may_overlap_player = false,
    bool actor_ignores_terrain = false);
bool start_world_player_motion(
    WorldState& world, const std::vector<WorldPathCommand>& player_path,
    std::string& error);
bool step_world_script_motion(WorldState& world, std::string& error);
std::uint8_t next_world_random_byte(WorldState& world);
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
