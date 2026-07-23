#pragma once

#include "catalog.hpp"
#include "diagnostics.hpp"
#include "sexpr.hpp"

#include <cstdint>
#include <vector>

namespace pokered {

struct AnimationNode {
    Symbol name;
    content::SceneNodeKind kind{content::SceneNodeKind::sprite};
    float x{};
    float y{};
    std::int32_t layer{};
    bool visible{};
    content::CoordinateSpace space{content::CoordinateSpace::native_canvas};
};

struct AnimationTween {
    std::uint32_t node{};
    std::uint32_t begin_tick{};
    std::uint32_t duration{};
    float from_x{};
    float from_y{};
    float to_x{};
    float to_y{};
    content::AnimationEase ease{content::AnimationEase::linear};
    content::CoordinateSpace space{content::CoordinateSpace::native_canvas};
};

enum class AnimationCueKind {
    sound,
    signal,
};

struct AnimationCue {
    AnimationCueKind kind{AnimationCueKind::signal};
    content::SoundId sound;
    Symbol signal;
};

struct AnimationState {
    const content::AnimationProgram* program{};
    std::vector<AnimationNode> nodes;
    std::vector<AnimationTween> tweens;
    std::vector<AnimationCue> cues;
    std::uint32_t tick{};
    std::size_t next_event{};
    bool finished{true};
};

bool compile_animation(const sexpr::Form& source, const content::Catalog& catalog,
                       content::AnimationProgram& result, Diagnostics& diagnostics);
bool start_animation(const content::AnimationProgram& program, const content::Catalog& catalog,
                     AnimationState& state, Diagnostics& diagnostics);
void step_animation(AnimationState& state);
const AnimationNode* find_animation_node(const AnimationState& state, const Symbol& name);

} // namespace pokered
