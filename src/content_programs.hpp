#pragma once

#include "content_ids.hpp"
#include "source.hpp"

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
    CompiledProgram program;
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

struct AnimationProgram {
    SceneId scene;
    CompiledProgram program;
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
