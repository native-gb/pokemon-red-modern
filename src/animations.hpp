#pragma once

#include "catalog.hpp"
#include "diagnostics.hpp"
#include "sexpr.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace pokered {

struct AnimationTarget {
    Symbol name;
    float x{};
    float y{};
    float offset_x{};
    float offset_y{};
    bool visible{};
    content::CoordinateSpace space{content::CoordinateSpace::native_canvas};
    content::AnimationPalette palette{content::AnimationPalette::normal};
    content::AnimationForm form{content::AnimationForm::normal};
    std::uint8_t squish_half_steps{};
    std::int16_t wave_phase{-1};
};

struct AnimationTween {
    std::uint32_t subject{};
    std::uint32_t begin_tick{};
    std::uint32_t duration{};
    float from_x{};
    float from_y{};
    float to_x{};
    float to_y{};
    content::AnimationEase ease{content::AnimationEase::linear};
    content::CoordinateSpace space{content::CoordinateSpace::native_canvas};
    bool offset{};
};

struct AnimationEffect {
    Symbol name;
    Symbol visual;
    float x{};
    float y{};
    bool visible{};
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
    std::vector<AnimationTarget> targets;
    std::vector<AnimationEffect> effects;
    std::vector<AnimationTween> tweens;
    std::vector<AnimationCue> cues;
    std::uint32_t tick{};
    std::size_t next_event{};
    bool finished{true};
};

bool compile_animation(const sexpr::Form& source, const content::Catalog& catalog,
                       content::AnimationProgram& result, Diagnostics& diagnostics);
bool start_animation(const content::AnimationProgram& program,
                     std::span<const AnimationTarget> targets, AnimationState& state,
                     Diagnostics& diagnostics);
void step_animation(AnimationState& state);
const AnimationTarget* find_animation_target(const AnimationState& state, const Symbol& name);
const AnimationEffect* find_animation_effect(const AnimationState& state, const Symbol& name);

} // namespace pokered
