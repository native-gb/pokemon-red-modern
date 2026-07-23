#pragma once

#include "content_assets.hpp"
#include "content_battle.hpp"
#include "content_index.hpp"
#include "content_programs.hpp"
#include "content_world.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pokered::content {

enum class PackState {
    absent,
    importing,
    partial,
    ready,
    incompatible,
};

struct CatalogSummary {
    PackState state{PackState::absent};
    std::string campaign{"No campaign loaded"};
    std::string source{"No local cartridge imported"};
    std::size_t maps{};
    std::size_t scripts{};
    std::size_t species{};
    std::size_t moves{};
    std::size_t items{};
};

struct DomainHash {
    Symbol domain;
    std::string sha256;
};

struct Manifest {
    std::uint32_t pack_version{};
    std::uint32_t schema_version{};
    std::uint32_t importer_version{};
    std::uint32_t compiler_version{};
    Symbol campaign;
    Symbol language;
    std::string source_sha1;
    std::vector<DomainHash> domains;
};

struct Provenance {
    Symbol record;
    Symbol source_profile;
    std::uint32_t source_bank{};
    std::uint32_t source_begin{};
    std::uint32_t source_end{};
    SourceSpan generated_source;
    Symbol package;
};

struct Catalog {
    Manifest manifest;

    Index<TextId, TextDef> texts;
    Index<FontId, FontDef> fonts;
    Index<ChoiceId, ChoiceDef> choices;

    Index<TerrainId, TerrainDef> terrain;
    Index<TilesetId, TilesetDef> tilesets;
    Index<MapId, MapDef> maps;
    Index<ConnectionId, ConnectionDef> connections;
    Index<WarpId, WarpDef> warps;
    Index<TriggerId, TriggerDef> triggers;
    Index<ActorDefId, ActorDef> actor_defs;
    Index<ActorSpawnId, ActorSpawnDef> actor_spawns;
    Index<MovementPathId, MovementPathDef> movement_paths;

    Index<FlagId, FlagDef> flags;
    Index<VariableId, VariableDef> variables;
    Index<PredicateId, PredicateProgram> predicates;
    Index<ScriptId, ScriptProgram> scripts;

    Index<TypeId, TypeDef> types;
    Index<TypeInteractionId, TypeInteractionDef> type_interactions;
    Index<StatusId, StatusDef> statuses;
    Index<StatusImmunityId, StatusImmunityDef> status_immunities;
    Index<SpeciesId, SpeciesDef> species;
    Index<LearnsetEntryId, LearnsetEntryDef> learnset_entries;
    Index<EvolutionId, EvolutionDef> evolutions;
    Index<GrowthCurveId, GrowthCurveDef> growth_curves;
    Index<DexEntryId, DexEntryDef> dex_entries;

    Index<MoveId, MoveDef> moves;
    Index<BattleEffectId, BattleEffectProgram> battle_effects;
    Index<ItemId, ItemDef> items;
    Index<ItemEffectId, ItemEffectProgram> item_effects;
    Index<MachineId, MachineDef> machines;
    Index<ShopId, ShopDef> shops;

    Index<TrainerClassId, TrainerClassDef> trainer_classes;
    Index<TrainerPartyId, TrainerPartyDef> trainer_parties;
    Index<AiProgramId, AiProgram> ai_programs;
    Index<EncounterTableId, EncounterTableDef> encounter_tables;
    Index<EncounterSlotId, EncounterSlotDef> encounter_slots;
    Index<TradeId, TradeDef> trades;
    Index<GiftId, GiftDef> gifts;

    Index<ImageId, ImageDef> images;
    Index<SpriteId, SpriteDef> sprites;
    Index<SpriteClipId, SpriteClipDef> sprite_clips;
    Index<PaletteId, PaletteDef> palettes;
    Index<AnimationId, AnimationProgram> animations;

    Index<InstrumentId, InstrumentDef> instruments;
    Index<MusicId, MusicProgram> music;
    Index<SoundId, SoundProgram> sounds;
    Index<CryId, CryProgram> cries;

    Index<UiStyleId, UiStyleDef> ui_styles;
    Index<UiLayoutId, UiLayoutDef> ui_layouts;
    Index<MenuId, MenuDef> menus;
    Index<CreditsId, CreditsDef> credits;

    std::vector<Provenance> provenance;
};

const char* label(PackState state);
CatalogSummary summarize(const Catalog& catalog, PackState state);
bool validate_catalog(const Catalog& catalog, Diagnostics& diagnostics);

} // namespace pokered::content
