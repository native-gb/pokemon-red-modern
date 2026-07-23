#pragma once

#include "content_ids.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pokered::content {

enum class Direction {
    down,
    up,
    left,
    right,
};

enum class Environment {
    outdoor,
    indoor,
    cave,
    dungeon,
};

enum class TerrainRole {
    ordinary,
    wall,
    grass,
    water,
    ledge,
    counter,
    door,
    stairs,
    warp,
};

enum class ActorMovement {
    still,
    wander,
    turn,
    path,
    follow,
};

enum class TransitionKind {
    none,
    door,
    stairs,
    cave,
    teleport,
    fall,
};

struct GridPosition {
    std::int32_t x{};
    std::int32_t y{};
};

struct GridArea {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{1};
    std::int32_t height{1};
};

struct Color {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
    std::uint8_t alpha{255};
};

struct TextToken {
    enum class Kind {
        literal,
        text,
        player_name,
        rival_name,
        pokemon_name,
        item_name,
        move_name,
        number,
        page,
        line_break,
    };

    Kind kind{Kind::literal};
    std::string literal;
    TextId text;
};

struct TextDef {
    std::vector<TextToken> tokens;
    TextId fallback;
    FontId font;
};

struct GlyphDef {
    std::uint32_t codepoint{};
    SpriteId sprite;
    std::int32_t advance{};
};

struct FontDef {
    std::vector<GlyphDef> glyphs;
    std::int32_t line_height{};
    std::int32_t default_advance{};
};

struct ChoiceOptionDef {
    TextId text;
    std::int32_t value{};
    PredicateId enabled_when;
};

struct ChoiceDef {
    std::vector<ChoiceOptionDef> options;
    std::uint32_t default_option{};
    bool cancel_allowed{};
};

struct TerrainDef {
    TerrainRole role{TerrainRole::ordinary};
    bool passable{};
    bool encounter_eligible{};
    Direction ledge_direction{Direction::down};
};

struct TilesetDef {
    ImageId image;
    std::int32_t tile_width{8};
    std::int32_t tile_height{8};
    std::vector<TerrainId> tile_terrain;
};

struct MapDef {
    TextId name;
    TilesetId tileset;
    Environment environment{Environment::outdoor};
    std::int32_t width{};
    std::int32_t height{};
    std::vector<std::uint32_t> cells;
    std::vector<TerrainId> collision;
    MusicId music;
    EncounterTableId encounter_table;
    IdRange<ConnectionId> connections;
    IdRange<WarpId> warps;
    IdRange<TriggerId> triggers;
    IdRange<ActorSpawnId> actor_spawns;
};

struct ConnectionDef {
    MapId from;
    Direction edge{Direction::up};
    MapId to;
    Direction destination_edge{Direction::down};
    std::int32_t offset{};
};

struct WarpDef {
    MapId map;
    GridArea area;
    MapId destination;
    GridPosition spawn;
    Direction facing{Direction::down};
    TransitionKind transition{TransitionKind::none};
    PredicateId enabled_when;
};

struct TriggerDef {
    MapId map;
    GridArea area;
    PredicateId enabled_when;
    ScriptId script;
    bool once{};
};

struct ActorDef {
    TextId name;
    SpriteId sprite;
    PaletteId palette;
    bool solid{true};
    ActorMovement movement{ActorMovement::still};
    ScriptId default_interaction;
};

struct ActorSpawnDef {
    MapId map;
    ActorDefId actor;
    GridPosition position;
    Direction facing{Direction::down};
    PredicateId visible_when;
    ScriptId interaction;
    TrainerPartyId trainer_party;
    std::uint32_t sight_range{};
};

struct MovementStepDef {
    Direction direction{Direction::down};
    std::uint32_t tiles{1};
    std::uint32_t pace{1};
};

struct MovementPathDef {
    std::vector<MovementStepDef> steps;
};

struct FlagDef {
    bool default_value{};
};

struct VariableDef {
    std::int32_t default_value{};
    std::int32_t minimum{};
    std::int32_t maximum{};
};

} // namespace pokered::content
