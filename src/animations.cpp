#include "animations.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace pokered {
namespace {

struct TimelineCompiler {
    const content::Catalog& catalog;
    content::AnimationProgram program;
    Diagnostics& diagnostics;
};

const Symbol* symbol_argument(const sexpr::Form& form, std::size_t index,
                              Diagnostics& diagnostics) {
    const sexpr::Atom* atom = sexpr::argument(form, index);
    if (atom != nullptr && atom->kind == sexpr::AtomKind::symbol) return &atom->symbol;
    add_error(diagnostics, form.source, "animation_symbol_argument",
              "'" + form.head.symbol.text + "' requires a symbol at argument " +
                  std::to_string(index + 1));
    return nullptr;
}

std::optional<std::int64_t> integer_argument(const sexpr::Form& form, std::size_t index,
                                             Diagnostics& diagnostics) {
    const sexpr::Atom* atom = sexpr::argument(form, index);
    if (atom != nullptr && atom->kind == sexpr::AtomKind::integer) return atom->integer;
    add_error(diagnostics, form.source, "animation_integer_argument",
              "'" + form.head.symbol.text + "' requires an integer at argument " +
                  std::to_string(index + 1));
    return std::nullopt;
}

bool exact_arguments(const sexpr::Form& form, std::size_t count, Diagnostics& diagnostics) {
    if (form.arguments.size() == count) return true;
    add_error(diagnostics, form.source, "animation_argument_count",
              "'" + form.head.symbol.text + "' requires " + std::to_string(count) +
                  " inline argument(s)");
    return false;
}

std::optional<std::uint32_t> intern_symbol(const Symbol& symbol, TimelineCompiler& compiler) {
    const auto found =
        std::find(compiler.program.symbols.begin(), compiler.program.symbols.end(), symbol);
    if (found != compiler.program.symbols.end()) {
        return static_cast<std::uint32_t>(std::distance(compiler.program.symbols.begin(), found));
    }
    if (compiler.program.symbols.size() >= std::numeric_limits<std::uint32_t>::max()) {
        add_error(compiler.diagnostics, {}, "animation_symbol_overflow",
                  "animation has too many symbols");
        return std::nullopt;
    }
    compiler.program.symbols.push_back(symbol);
    return static_cast<std::uint32_t>(compiler.program.symbols.size() - 1);
}

std::optional<content::CoordinateSpace> coordinate_space(const Symbol& symbol) {
    if (symbol.text == "native_canvas") return content::CoordinateSpace::native_canvas;
    if (symbol.text == "viewport") return content::CoordinateSpace::viewport;
    if (symbol.text == "world") return content::CoordinateSpace::world;
    if (symbol.text == "node_local") return content::CoordinateSpace::node_local;
    return std::nullopt;
}

std::optional<content::AnimationEase> easing(const Symbol& symbol) {
    if (symbol.text == "linear") return content::AnimationEase::linear;
    if (symbol.text == "ease_in") return content::AnimationEase::ease_in;
    if (symbol.text == "ease_out") return content::AnimationEase::ease_out;
    if (symbol.text == "ease_in_out") return content::AnimationEase::ease_in_out;
    return std::nullopt;
}

bool integer_fits_i32(std::int64_t value) {
    return value >= std::numeric_limits<std::int32_t>::min() &&
           value <= std::numeric_limits<std::int32_t>::max();
}

bool integer_fits_u32(std::int64_t value) {
    return value >= 0 &&
           static_cast<std::uint64_t>(value) <= std::numeric_limits<std::uint32_t>::max();
}

std::uint32_t compile_statement(const sexpr::Form& form, std::uint32_t at_tick,
                                TimelineCompiler& compiler);

std::uint32_t compile_sequence(const std::vector<sexpr::Form>& forms, std::uint32_t at_tick,
                               TimelineCompiler& compiler) {
    std::uint32_t cursor = at_tick;
    for (const sexpr::Form& form : forms) {
        const std::uint32_t duration = compile_statement(form, cursor, compiler);
        if (duration > std::numeric_limits<std::uint32_t>::max() - cursor) {
            add_error(compiler.diagnostics, form.source, "animation_duration_overflow",
                      "animation timeline exceeds the supported duration");
            return 0;
        }
        cursor += duration;
    }
    return cursor - at_tick;
}

std::uint32_t compile_parallel(const std::vector<sexpr::Form>& forms, std::uint32_t at_tick,
                               TimelineCompiler& compiler) {
    std::uint32_t duration = 0;
    for (const sexpr::Form& form : forms)
        duration = std::max(duration, compile_statement(form, at_tick, compiler));
    return duration;
}

std::uint32_t compile_visibility(const sexpr::Form& form, std::uint32_t at_tick,
                                 TimelineCompiler& compiler, bool visible) {
    if (!exact_arguments(form, 1, compiler.diagnostics)) return 0;
    const Symbol* node = symbol_argument(form, 0, compiler.diagnostics);
    if (node == nullptr) return 0;
    const auto subject = intern_symbol(*node, compiler);
    if (!subject) return 0;
    compiler.program.events.push_back({
        .operation = visible ? content::AnimationOp::show : content::AnimationOp::hide,
        .at_tick = at_tick,
        .subject = *subject,
        .visual = 0,
        .sound = {},
        .source = form.source,
    });
    return 0;
}

std::uint32_t compile_set_position(const sexpr::Form& form, std::uint32_t at_tick,
                                   TimelineCompiler& compiler, bool offset) {
    if (!exact_arguments(form, 4, compiler.diagnostics)) return 0;
    const Symbol* node = symbol_argument(form, 0, compiler.diagnostics);
    const auto x = integer_argument(form, 1, compiler.diagnostics);
    const auto y = integer_argument(form, 2, compiler.diagnostics);
    const Symbol* space_name = symbol_argument(form, 3, compiler.diagnostics);
    if (node == nullptr || !x || !y || space_name == nullptr) return 0;
    const auto subject = intern_symbol(*node, compiler);
    const auto space = coordinate_space(*space_name);
    if (!subject || !space || !integer_fits_i32(*x) || !integer_fits_i32(*y)) {
        add_error(compiler.diagnostics, form.source, "invalid_animation_position",
                  form.head.symbol.text +
                      " uses an unknown space or out-of-range coordinate");
        return 0;
    }
    compiler.program.events.push_back({
        .operation =
            offset ? content::AnimationOp::set_offset : content::AnimationOp::set_position,
        .at_tick = at_tick,
        .subject = *subject,
        .visual = 0,
        .x = static_cast<std::int32_t>(*x),
        .y = static_cast<std::int32_t>(*y),
        .space = *space,
        .sound = {},
        .source = form.source,
    });
    return 0;
}

std::uint32_t compile_tween_position(const sexpr::Form& form, std::uint32_t at_tick,
                                     TimelineCompiler& compiler, bool offset) {
    if (!exact_arguments(form, 6, compiler.diagnostics)) return 0;
    const Symbol* node = symbol_argument(form, 0, compiler.diagnostics);
    const auto x = integer_argument(form, 1, compiler.diagnostics);
    const auto y = integer_argument(form, 2, compiler.diagnostics);
    const auto duration = integer_argument(form, 3, compiler.diagnostics);
    const Symbol* ease_name = symbol_argument(form, 4, compiler.diagnostics);
    const Symbol* space_name = symbol_argument(form, 5, compiler.diagnostics);
    if (node == nullptr || !x || !y || !duration || ease_name == nullptr || space_name == nullptr) {
        return 0;
    }
    const auto subject = intern_symbol(*node, compiler);
    const auto ease = easing(*ease_name);
    const auto space = coordinate_space(*space_name);
    if (!subject || !ease || !space || !integer_fits_i32(*x) || !integer_fits_i32(*y) ||
        !integer_fits_u32(*duration) || *duration == 0) {
        add_error(compiler.diagnostics, form.source, "invalid_animation_tween",
                  form.head.symbol.text +
                      " uses an invalid coordinate, duration, easing, or space");
        return 0;
    }
    compiler.program.events.push_back({
        .operation =
            offset ? content::AnimationOp::tween_offset : content::AnimationOp::tween_position,
        .at_tick = at_tick,
        .duration = static_cast<std::uint32_t>(*duration),
        .subject = *subject,
        .visual = 0,
        .x = static_cast<std::int32_t>(*x),
        .y = static_cast<std::int32_t>(*y),
        .space = *space,
        .ease = *ease,
        .sound = {},
        .source = form.source,
    });
    return static_cast<std::uint32_t>(*duration);
}

std::uint32_t compile_statement(const sexpr::Form& form, std::uint32_t at_tick,
                                TimelineCompiler& compiler) {
    if (form.head.kind != sexpr::AtomKind::symbol) {
        add_error(compiler.diagnostics, form.source, "animation_literal_statement",
                  "animation statements must begin with an operation");
        return 0;
    }

    const std::string& operation = form.head.symbol.text;
    if (operation == "sequence") {
        if (!form.arguments.empty()) {
            add_error(compiler.diagnostics, form.source, "animation_sequence_arguments",
                      "sequence accepts only indented statements");
            return 0;
        }
        return compile_sequence(form.children, at_tick, compiler);
    }
    if (operation == "parallel") {
        if (!form.arguments.empty()) {
            add_error(compiler.diagnostics, form.source, "animation_parallel_arguments",
                      "parallel accepts only indented statements");
            return 0;
        }
        return compile_parallel(form.children, at_tick, compiler);
    }
    if (operation == "wait") {
        if (!exact_arguments(form, 1, compiler.diagnostics)) return 0;
        const auto duration = integer_argument(form, 0, compiler.diagnostics);
        if (!duration || !integer_fits_u32(*duration)) {
            add_error(compiler.diagnostics, form.source, "invalid_animation_wait",
                      "wait duration must be a nonnegative 32-bit integer");
            return 0;
        }
        return static_cast<std::uint32_t>(*duration);
    }
    if (operation == "show") return compile_visibility(form, at_tick, compiler, true);
    if (operation == "hide") return compile_visibility(form, at_tick, compiler, false);
    if (operation == "spawn") {
        if (!exact_arguments(form, 2, compiler.diagnostics)) return 0;
        const Symbol* effect = symbol_argument(form, 0, compiler.diagnostics);
        const Symbol* visual = symbol_argument(form, 1, compiler.diagnostics);
        if (effect == nullptr || visual == nullptr) return 0;
        const auto subject = intern_symbol(*effect, compiler);
        const auto visual_index = intern_symbol(*visual, compiler);
        if (!subject || !visual_index) return 0;
        compiler.program.events.push_back({
            .operation = content::AnimationOp::spawn,
            .at_tick = at_tick,
            .subject = *subject,
            .visual = *visual_index,
            .sound = {},
            .source = form.source,
        });
        return 0;
    }
    if (operation == "destroy") {
        if (!exact_arguments(form, 1, compiler.diagnostics)) return 0;
        const Symbol* effect = symbol_argument(form, 0, compiler.diagnostics);
        if (effect == nullptr) return 0;
        const auto subject = intern_symbol(*effect, compiler);
        if (!subject) return 0;
        compiler.program.events.push_back({
            .operation = content::AnimationOp::destroy,
            .at_tick = at_tick,
            .subject = *subject,
            .visual = 0,
            .sound = {},
            .source = form.source,
        });
        return 0;
    }
    if (operation == "set_position") return compile_set_position(form, at_tick, compiler, false);
    if (operation == "tween_position")
        return compile_tween_position(form, at_tick, compiler, false);
    if (operation == "set_offset") return compile_set_position(form, at_tick, compiler, true);
    if (operation == "tween_offset")
        return compile_tween_position(form, at_tick, compiler, true);
    if (operation == "play_sound") {
        if (!exact_arguments(form, 1, compiler.diagnostics)) return 0;
        const Symbol* sound_name = symbol_argument(form, 0, compiler.diagnostics);
        if (sound_name == nullptr) return 0;
        const auto sound = content::find(compiler.catalog.sounds, *sound_name);
        if (!sound) {
            add_error(compiler.diagnostics, form.source, "unknown_animation_sound",
                      "unknown sound '" + sound_name->text + "'");
            return 0;
        }
        compiler.program.events.push_back({
            .operation = content::AnimationOp::play_sound,
            .at_tick = at_tick,
            .visual = 0,
            .sound = *sound,
            .source = form.source,
        });
        return 0;
    }
    if (operation == "signal") {
        if (!exact_arguments(form, 1, compiler.diagnostics)) return 0;
        const Symbol* signal = symbol_argument(form, 0, compiler.diagnostics);
        if (signal == nullptr) return 0;
        const auto subject = intern_symbol(*signal, compiler);
        if (!subject) return 0;
        compiler.program.events.push_back({
            .operation = content::AnimationOp::signal,
            .at_tick = at_tick,
            .subject = *subject,
            .visual = 0,
            .sound = {},
            .source = form.source,
        });
        return 0;
    }

    add_error(compiler.diagnostics, form.source, "unknown_animation_operation",
              "unknown animation operation '" + operation + "'");
    return 0;
}

float eased(float progress, content::AnimationEase ease) {
    const float clamped = std::clamp(progress, 0.0F, 1.0F);
    if (ease == content::AnimationEase::ease_in) return clamped * clamped;
    if (ease == content::AnimationEase::ease_out) return 1.0F - (1.0F - clamped) * (1.0F - clamped);
    if (ease == content::AnimationEase::ease_in_out)
        return clamped < 0.5F ? 2.0F * clamped * clamped
                              : 1.0F - std::pow(-2.0F * clamped + 2.0F, 2.0F) * 0.5F;
    return clamped;
}

std::optional<std::size_t> runtime_target_index(const AnimationState& state, const Symbol& name) {
    const auto found =
        std::find_if(state.targets.begin(), state.targets.end(),
                     [&name](const AnimationTarget& target) { return target.name == name; });
    if (found == state.targets.end()) return std::nullopt;
    return static_cast<std::size_t>(std::distance(state.targets.begin(), found));
}

std::optional<std::size_t> runtime_effect_index(const AnimationState& state, const Symbol& name) {
    const auto found =
        std::find_if(state.effects.begin(), state.effects.end(),
                     [&name](const AnimationEffect& effect) { return effect.name == name; });
    if (found == state.effects.end()) return std::nullopt;
    return static_cast<std::size_t>(std::distance(state.effects.begin(), found));
}

void apply_event(const content::AnimationEvent& event, AnimationState& state) {
    const content::AnimationProgram& program = *state.program;
    if (event.operation == content::AnimationOp::play_sound) {
        AnimationCue cue;
        cue.kind = AnimationCueKind::sound;
        cue.sound = event.sound;
        state.cues.push_back(std::move(cue));
        return;
    }
    if (event.operation == content::AnimationOp::signal) {
        AnimationCue cue;
        cue.kind = AnimationCueKind::signal;
        cue.signal = program.symbols[event.subject];
        state.cues.push_back(std::move(cue));
        return;
    }
    if (event.operation == content::AnimationOp::spawn) {
        const Symbol& name = program.symbols[event.subject];
        const auto existing = runtime_effect_index(state, name);
        if (existing)
            state.effects.erase(state.effects.begin() + static_cast<std::ptrdiff_t>(*existing));
        AnimationEffect effect;
        effect.name = name;
        effect.visual = program.symbols[event.visual];
        effect.visible = true;
        state.effects.push_back(std::move(effect));
        return;
    }
    if (event.operation == content::AnimationOp::destroy) {
        const auto existing = runtime_effect_index(state, program.symbols[event.subject]);
        if (existing)
            state.effects.erase(state.effects.begin() + static_cast<std::ptrdiff_t>(*existing));
        return;
    }

    const Symbol& subject = program.symbols[event.subject];
    const auto target_index = runtime_target_index(state, subject);
    const auto effect_index = runtime_effect_index(state, subject);
    if (!target_index && !effect_index) return;

    // Persistent target offsets remain separate from renderer-owned base positions.
    const bool offset_operation = event.operation == content::AnimationOp::set_offset ||
                                  event.operation == content::AnimationOp::tween_offset;
    if (offset_operation && !target_index) return;
    float* x = target_index
                   ? (offset_operation ? &state.targets[*target_index].offset_x
                                       : &state.targets[*target_index].x)
                   : &state.effects[*effect_index].x;
    float* y = target_index
                   ? (offset_operation ? &state.targets[*target_index].offset_y
                                       : &state.targets[*target_index].y)
                   : &state.effects[*effect_index].y;
    bool* visible = target_index ? &state.targets[*target_index].visible
                                 : &state.effects[*effect_index].visible;
    content::CoordinateSpace* space =
        target_index ? &state.targets[*target_index].space : &state.effects[*effect_index].space;
    if (event.operation == content::AnimationOp::show)
        *visible = true;
    else if (event.operation == content::AnimationOp::hide)
        *visible = false;
    else if (event.operation == content::AnimationOp::set_position ||
             event.operation == content::AnimationOp::set_offset) {
        *x = static_cast<float>(event.x);
        *y = static_cast<float>(event.y);
        *space = event.space;
    } else if (event.operation == content::AnimationOp::tween_position ||
               event.operation == content::AnimationOp::tween_offset) {
        state.tweens.erase(std::remove_if(state.tweens.begin(), state.tweens.end(),
                                          [&event](const AnimationTween& tween) {
                                              return tween.subject == event.subject;
                                          }),
                           state.tweens.end());
        state.tweens.push_back({
            .subject = event.subject,
            .begin_tick = event.at_tick,
            .duration = event.duration,
            .from_x = *x,
            .from_y = *y,
            .to_x = static_cast<float>(event.x),
            .to_y = static_cast<float>(event.y),
            .ease = event.ease,
            .space = event.space,
            .offset = offset_operation,
        });
        *space = event.space;
    }
}

} // namespace

bool compile_animation(const sexpr::Form& source, const content::Catalog& catalog,
                       content::AnimationProgram& result, Diagnostics& diagnostics) {
    if (!sexpr::is_head(source, "animation") || source.arguments.size() != 1 ||
        source.arguments.front().kind != sexpr::AtomKind::symbol) {
        add_error(diagnostics, source.source, "invalid_animation_definition",
                  "animation requires exactly one symbolic name");
        return false;
    }

    // Lower view-independent sequence and parallel blocks into one deterministic timeline.
    TimelineCompiler compiler{
        .catalog = catalog,
        .program = {},
        .diagnostics = diagnostics,
    };
    compiler.program.duration = compile_sequence(source.children, 0, compiler);
    std::stable_sort(compiler.program.events.begin(), compiler.program.events.end(),
                     [](const content::AnimationEvent& left, const content::AnimationEvent& right) {
                         return left.at_tick < right.at_tick;
                     });
    if (!diagnostics.ok()) return false;
    result = std::move(compiler.program);
    return true;
}

bool start_animation(const content::AnimationProgram& program,
                     std::span<const AnimationTarget> targets, AnimationState& state,
                     Diagnostics& diagnostics) {
    std::vector<Symbol> available;
    available.reserve(targets.size() + program.events.size());
    for (const AnimationTarget& target : targets)
        available.push_back(target.name);

    // Validate each timeline reference against view targets or effects spawned earlier.
    for (const content::AnimationEvent& event : program.events) {
        if (event.operation == content::AnimationOp::play_sound ||
            event.operation == content::AnimationOp::signal) {
            continue;
        }
        const Symbol& name = program.symbols[event.subject];
        if (event.operation == content::AnimationOp::spawn) {
            available.push_back(name);
            continue;
        }
        const bool found = std::find(available.begin(), available.end(), name) != available.end();
        if (!found) {
            add_error(diagnostics, event.source, "missing_animation_target",
                      "active view does not expose animation target '" + name.text + "'");
            return false;
        }
    }

    // Copy only the small transform overrides which the active view reads while drawing.
    AnimationState started;
    started.program = &program;
    started.finished = false;
    started.targets.assign(targets.begin(), targets.end());
    state = std::move(started);
    return true;
}

void step_animation(AnimationState& state) {
    if (state.finished || state.program == nullptr) return;
    state.cues.clear();

    // Start every event scheduled for this tick before sampling active tweens.
    while (state.next_event < state.program->events.size() &&
           state.program->events[state.next_event].at_tick == state.tick) {
        apply_event(state.program->events[state.next_event], state);
        ++state.next_event;
    }
    for (const AnimationTween& tween : state.tweens) {
        if (tween.subject >= state.program->symbols.size()) continue;
        const Symbol& subject = state.program->symbols[tween.subject];
        const auto target_index = runtime_target_index(state, subject);
        const auto effect_index = runtime_effect_index(state, subject);
        if (!target_index && !effect_index) continue;
        const float progress =
            static_cast<float>(state.tick - tween.begin_tick) / static_cast<float>(tween.duration);
        const float amount = eased(progress, tween.ease);
        float* x = target_index
                       ? (tween.offset ? &state.targets[*target_index].offset_x
                                       : &state.targets[*target_index].x)
                       : &state.effects[*effect_index].x;
        float* y = target_index
                       ? (tween.offset ? &state.targets[*target_index].offset_y
                                       : &state.targets[*target_index].y)
                       : &state.effects[*effect_index].y;
        *x = tween.from_x + (tween.to_x - tween.from_x) * amount;
        *y = tween.from_y + (tween.to_y - tween.from_y) * amount;
    }
    state.tweens.erase(std::remove_if(state.tweens.begin(), state.tweens.end(),
                                      [&state](const AnimationTween& tween) {
                                          return state.tick >= tween.begin_tick + tween.duration;
                                      }),
                       state.tweens.end());

    // The final duration tick applies terminal tween values and cues before completion.
    state.finished = state.tick >= state.program->duration &&
                     state.next_event >= state.program->events.size() && state.tweens.empty();
    ++state.tick;
}

const AnimationTarget* find_animation_target(const AnimationState& state, const Symbol& name) {
    const auto found = runtime_target_index(state, name);
    return found ? &state.targets[*found] : nullptr;
}

const AnimationEffect* find_animation_effect(const AnimationState& state, const Symbol& name) {
    const auto found = runtime_effect_index(state, name);
    return found ? &state.effects[*found] : nullptr;
}

} // namespace pokered
