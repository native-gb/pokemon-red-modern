#pragma once

#include "content_ids.hpp"
#include "source.hpp"
#include "symbols.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pokered::content {

enum class ProgramKind {
    predicate,
    campaign,
    battle_effect,
    battle_ai,
    animation,
    music,
    sound,
    cry,
};

enum class PredicateOp : std::uint8_t {
    push_integer,
    push_boolean,
    equal,
    not_equal,
    less,
    less_or_equal,
    greater,
    greater_or_equal,
    logical_and,
    logical_or,
    logical_not,
    add,
    subtract,
    multiply,
    divide,
    minimum,
    maximum,
    flag_set,
    variable,
    has_item,
    item_count,
    party_has_space,
    party_contains,
    species_seen,
    species_owned,
    actor_visible,
    current_map,
    player_level,
    friendship,
    battle_result,
};

struct PredicateInstruction {
    PredicateOp operation{PredicateOp::push_boolean};
    std::int64_t operand{};
    SourceSpan source;
};

struct SourceMapEntry {
    std::uint32_t instruction_begin{};
    std::uint32_t instruction_end{};
    SourceSpan source;
    std::uint32_t source_bank{};
    std::uint32_t source_begin{};
    std::uint32_t source_end{};
};

struct CompiledProgram {
    ProgramKind kind{ProgramKind::campaign};
    std::uint32_t isa_version{};
    std::uint32_t maximum_call_depth{};
    std::uint32_t local_slots{};
    std::vector<std::uint8_t> bytecode;
    std::vector<std::string> symbols;
    std::vector<std::int64_t> integers;
    std::vector<SourceMapEntry> source_map;
};

struct PredicateProgram {
    std::vector<PredicateInstruction> instructions;
    std::uint32_t maximum_stack{};
};

struct ScriptProgram {
    CompiledProgram program;
};

struct BattleEffectProgram {
    CompiledProgram program;
};

struct AiProgram {
    CompiledProgram program;
};

enum class CoordinateSpace : std::uint8_t {
    native_canvas,
    viewport,
    world,
    node_local,
};

enum class AnimationEase : std::uint8_t {
    linear,
    ease_in,
    ease_out,
    ease_in_out,
};

enum class AnimationOp : std::uint8_t {
    show,
    hide,
    set_position,
    tween_position,
    play_sound,
    signal,
};

struct AnimationEvent {
    AnimationOp operation{AnimationOp::show};
    std::uint32_t at_tick{};
    std::uint32_t duration{};
    std::uint32_t subject{};
    std::int32_t x{};
    std::int32_t y{};
    CoordinateSpace space{CoordinateSpace::native_canvas};
    AnimationEase ease{AnimationEase::linear};
    SoundId sound;
    SourceSpan source;
};

struct AnimationProgram {
    std::vector<Symbol> symbols;
    std::vector<AnimationEvent> events;
    std::uint32_t duration{};
};

struct MusicProgram {
    CompiledProgram program;
};

struct SoundProgram {
    CompiledProgram program;
};

struct CryProgram {
    CompiledProgram program;
};

} // namespace pokered::content
