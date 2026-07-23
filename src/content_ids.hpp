#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace pokered::content {

template <class Tag> struct Id {
    static constexpr std::uint32_t invalid_value = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t value{invalid_value};

    bool valid() const {
        return value != invalid_value;
    }
    explicit operator bool() const {
        return valid();
    }
    auto operator<=>(const Id&) const = default;
};

template <class IdType> struct IdRange {
    std::uint32_t begin{};
    std::uint32_t count{};

    bool empty() const {
        return count == 0;
    }
};

using TextId = Id<struct TextTag>;
using FontId = Id<struct FontTag>;
using ChoiceId = Id<struct ChoiceTag>;
using TerrainId = Id<struct TerrainTag>;
using TilesetId = Id<struct TilesetTag>;
using MapId = Id<struct MapTag>;
using ConnectionId = Id<struct ConnectionTag>;
using WarpId = Id<struct WarpTag>;
using TriggerId = Id<struct TriggerTag>;
using ActorDefId = Id<struct ActorDefTag>;
using ActorSpawnId = Id<struct ActorSpawnTag>;
using MovementPathId = Id<struct MovementPathTag>;
using FlagId = Id<struct FlagTag>;
using VariableId = Id<struct VariableTag>;
using PredicateId = Id<struct PredicateTag>;
using ScriptId = Id<struct ScriptTag>;
using TypeId = Id<struct TypeTag>;
using TypeInteractionId = Id<struct TypeInteractionTag>;
using StatusId = Id<struct StatusTag>;
using StatusImmunityId = Id<struct StatusImmunityTag>;
using SpeciesId = Id<struct SpeciesTag>;
using LearnsetEntryId = Id<struct LearnsetEntryTag>;
using EvolutionId = Id<struct EvolutionTag>;
using GrowthCurveId = Id<struct GrowthCurveTag>;
using DexEntryId = Id<struct DexEntryTag>;
using MoveId = Id<struct MoveTag>;
using BattleEffectId = Id<struct BattleEffectTag>;
using ItemId = Id<struct ItemTag>;
using ItemEffectId = Id<struct ItemEffectTag>;
using MachineId = Id<struct MachineTag>;
using ShopId = Id<struct ShopTag>;
using TrainerClassId = Id<struct TrainerClassTag>;
using TrainerPartyId = Id<struct TrainerPartyTag>;
using AiProgramId = Id<struct AiProgramTag>;
using EncounterTableId = Id<struct EncounterTableTag>;
using EncounterSlotId = Id<struct EncounterSlotTag>;
using TradeId = Id<struct TradeTag>;
using GiftId = Id<struct GiftTag>;
using ImageId = Id<struct ImageTag>;
using SpriteId = Id<struct SpriteTag>;
using SpriteClipId = Id<struct SpriteClipTag>;
using PaletteId = Id<struct PaletteTag>;
using SceneId = Id<struct SceneTag>;
using AnimationId = Id<struct AnimationTag>;
using InstrumentId = Id<struct InstrumentTag>;
using MusicId = Id<struct MusicTag>;
using SoundId = Id<struct SoundTag>;
using CryId = Id<struct CryTag>;
using UiStyleId = Id<struct UiStyleTag>;
using UiLayoutId = Id<struct UiLayoutTag>;
using MenuId = Id<struct MenuTag>;
using CreditsId = Id<struct CreditsTag>;

} // namespace pokered::content
