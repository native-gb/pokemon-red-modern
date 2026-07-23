#pragma once

#include "content_ids.hpp"
#include "content_programs.hpp"
#include "content_world.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace pokered::content {

enum class DamageClass {
    physical,
    special,
    status,
};

enum class EvolutionTrigger {
    level_up,
    item_use,
    trade,
    script,
};

enum class LearnMethod {
    starting,
    level_up,
    machine,
    tutor,
    script,
};

enum class ItemPocket {
    items,
    medicine,
    machines,
    key_items,
};

enum class EncounterKind {
    land,
    water,
    fishing,
    safari,
    static_encounter,
    gift,
    scripted,
};

struct Rational {
    std::uint16_t numerator{1};
    std::uint16_t denominator{1};
};

struct TypeDef {
    TextId name;
    DamageClass damage_class{DamageClass::physical};
    Color color;
};

struct TypeInteractionDef {
    TypeId attacking;
    TypeId defending;
    Rational multiplier;
};

struct StatusDef {
    TextId name;
    PredicateId can_apply;
    BattleEffectId turn_effect;
};

struct StatusImmunityDef {
    TypeId type;
    StatusId status;
};

struct SpeciesDef {
    std::uint32_t dex_number{};
    TextId name;
    TypeId primary_type;
    TypeId secondary_type;
    std::uint16_t base_hp{};
    std::uint16_t base_attack{};
    std::uint16_t base_defense{};
    std::uint16_t base_speed{};
    std::uint16_t base_special{};
    std::uint16_t catch_rate{};
    std::uint16_t experience_yield{};
    GrowthCurveId growth_curve;
    SpriteId front_sprite;
    SpriteId back_sprite;
    CryId cry;
    DexEntryId dex_entry;
    IdRange<LearnsetEntryId> learnset;
    IdRange<EvolutionId> evolutions;
};

struct LearnsetEntryDef {
    SpeciesId species;
    LearnMethod method{LearnMethod::level_up};
    std::uint16_t level{};
    MoveId move;
    std::uint32_t order{};
};

struct EvolutionDef {
    SpeciesId from;
    SpeciesId to;
    EvolutionTrigger trigger{EvolutionTrigger::level_up};
    PredicateId condition;
    ItemId item;
};

struct GrowthPointDef {
    std::uint16_t level{};
    std::uint32_t total_experience{};
};

struct GrowthCurveDef {
    std::vector<GrowthPointDef> points;
};

struct DexEntryDef {
    TextId category;
    TextId description;
    std::uint16_t height_decimeters{};
    std::uint32_t weight_hectograms{};
};

struct MoveDef {
    TextId name;
    TypeId type;
    DamageClass damage_class{DamageClass::physical};
    std::uint16_t power{};
    std::uint16_t accuracy{};
    std::uint16_t pp{};
    std::int16_t priority{};
    BattleEffectId effect;
    AnimationId animation;
    SoundId sound;
};

struct ItemDef {
    TextId name;
    TextId description;
    std::uint32_t price{};
    ItemPocket pocket{ItemPocket::items};
    ItemEffectId effect;
    std::int32_t parameter{};
    bool usable_in_battle{};
    bool usable_in_field{};
    bool key_item{};
};

struct ItemEffectProgram {
    CompiledProgram program;
};

struct MachineDef {
    ItemId item;
    MoveId teaches;
    bool reusable{};
};

struct ShopEntryDef {
    ItemId item;
    std::uint32_t price{};
    std::uint32_t order{};
    PredicateId enabled_when;
};

struct ShopDef {
    std::vector<ShopEntryDef> entries;
};

struct TrainerClassDef {
    TextId name;
    SpriteId front_sprite;
    std::uint32_t reward_factor{};
    MusicId encounter_music;
    MusicId battle_music;
};

struct TrainerMemberDef {
    SpeciesId species;
    std::uint16_t level{};
    std::array<MoveId, 4> moves;
};

struct TrainerPartyDef {
    TrainerClassId trainer_class;
    TextId name;
    AiProgramId ai;
    std::uint32_t reward_factor{};
    std::vector<TrainerMemberDef> members;
};

struct EncounterTableDef {
    EncounterKind kind{EncounterKind::land};
    std::uint16_t rate{};
    TerrainId terrain;
    IdRange<EncounterSlotId> slots;
};

struct EncounterSlotDef {
    EncounterTableId table;
    SpeciesId species;
    std::uint16_t minimum_level{};
    std::uint16_t maximum_level{};
    std::uint32_t weight{};
    std::uint32_t order{};
    PredicateId enabled_when;
};

struct TradeDef {
    SpeciesId requested_species;
    SpeciesId offered_species;
    TextId nickname;
    PredicateId enabled_when;
};

struct GiftDef {
    SpeciesId species;
    std::uint16_t level{};
    TextId nickname;
    PredicateId enabled_when;
};

} // namespace pokered::content
