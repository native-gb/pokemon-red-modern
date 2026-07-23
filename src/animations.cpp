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
    const content::SceneDef& scene;
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

std::optional<std::uint32_t> node_index(const sexpr::Form& form, const Symbol& name,
                                        TimelineCompiler& compiler) {
    const auto found =
        std::find_if(compiler.scene.nodes.begin(), compiler.scene.nodes.end(),
                     [&name](const content::SceneNodeDef& node) { return node.name == name; });
    if (found == compiler.scene.nodes.end()) {
        add_error(compiler.diagnostics, form.source, "unknown_animation_node",
                  "scene has no node named '" + name.text + "'");
        return std::nullopt;
    }

    // Store scene node names once so runtime events retain useful symbolic inspection.
    const auto symbol =
        std::find(compiler.program.symbols.begin(), compiler.program.symbols.end(), name);
    if (symbol != compiler.program.symbols.end()) {
        return static_cast<std::uint32_t>(std::distance(compiler.program.symbols.begin(), symbol));
    }
    compiler.program.symbols.push_back(name);
    return static_cast<std::uint32_t>(compiler.program.symbols.size() - 1);
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
    const auto subject = node_index(form, *node, compiler);
    if (!subject) return 0;
    compiler.program.events.push_back({
        .operation = visible ? content::AnimationOp::show : content::AnimationOp::hide,
        .at_tick = at_tick,
        .subject = *subject,
        .sound = {},
        .source = form.source,
    });
    return 0;
}

std::uint32_t compile_set_position(const sexpr::Form& form, std::uint32_t at_tick,
                                   TimelineCompiler& compiler) {
    if (!exact_arguments(form, 4, compiler.diagnostics)) return 0;
    const Symbol* node = symbol_argument(form, 0, compiler.diagnostics);
    const auto x = integer_argument(form, 1, compiler.diagnostics);
    const auto y = integer_argument(form, 2, compiler.diagnostics);
    const Symbol* space_name = symbol_argument(form, 3, compiler.diagnostics);
    if (node == nullptr || !x || !y || space_name == nullptr) return 0;
    const auto subject = node_index(form, *node, compiler);
    const auto space = coordinate_space(*space_name);
    if (!subject || !space || !integer_fits_i32(*x) || !integer_fits_i32(*y)) {
        add_error(compiler.diagnostics, form.source, "invalid_animation_position",
                  "set_position uses an unknown space or out-of-range coordinate");
        return 0;
    }
    compiler.program.events.push_back({
        .operation = content::AnimationOp::set_position,
        .at_tick = at_tick,
        .subject = *subject,
        .x = static_cast<std::int32_t>(*x),
        .y = static_cast<std::int32_t>(*y),
        .space = *space,
        .sound = {},
        .source = form.source,
    });
    return 0;
}

std::uint32_t compile_tween_position(const sexpr::Form& form, std::uint32_t at_tick,
                                     TimelineCompiler& compiler) {
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
    const auto subject = node_index(form, *node, compiler);
    const auto ease = easing(*ease_name);
    const auto space = coordinate_space(*space_name);
    if (!subject || !ease || !space || !integer_fits_i32(*x) || !integer_fits_i32(*y) ||
        !integer_fits_u32(*duration) || *duration == 0) {
        add_error(compiler.diagnostics, form.source, "invalid_animation_tween",
                  "tween_position uses an invalid coordinate, duration, easing, or space");
        return 0;
    }
    compiler.program.events.push_back({
        .operation = content::AnimationOp::tween_position,
        .at_tick = at_tick,
        .duration = static_cast<std::uint32_t>(*duration),
        .subject = *subject,
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
    if (operation == "set_position") return compile_set_position(form, at_tick, compiler);
    if (operation == "tween_position") return compile_tween_position(form, at_tick, compiler);
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

std::optional<std::size_t> runtime_node_index(const AnimationState& state, const Symbol& name) {
    const auto found =
        std::find_if(state.nodes.begin(), state.nodes.end(),
                     [&name](const AnimationNode& node) { return node.name == name; });
    if (found == state.nodes.end()) return std::nullopt;
    return static_cast<std::size_t>(std::distance(state.nodes.begin(), found));
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

    const auto index = runtime_node_index(state, program.symbols[event.subject]);
    if (!index) return;
    AnimationNode& node = state.nodes[*index];
    if (event.operation == content::AnimationOp::show)
        node.visible = true;
    else if (event.operation == content::AnimationOp::hide)
        node.visible = false;
    else if (event.operation == content::AnimationOp::set_position) {
        node.x = static_cast<float>(event.x);
        node.y = static_cast<float>(event.y);
        node.space = event.space;
    } else if (event.operation == content::AnimationOp::tween_position) {
        state.tweens.erase(
            std::remove_if(state.tweens.begin(), state.tweens.end(),
                           [index](const AnimationTween& tween) { return tween.node == *index; }),
            state.tweens.end());
        state.tweens.push_back({
            .node = static_cast<std::uint32_t>(*index),
            .begin_tick = event.at_tick,
            .duration = event.duration,
            .from_x = node.x,
            .from_y = node.y,
            .to_x = static_cast<float>(event.x),
            .to_y = static_cast<float>(event.y),
            .ease = event.ease,
            .space = event.space,
        });
        node.space = event.space;
    }
}

} // namespace

bool compile_animation(const sexpr::Form& source, const content::Catalog& catalog,
                       content::AnimationProgram& result, Diagnostics& diagnostics) {
    if (!sexpr::is_head(source, "animation") || source.arguments.size() != 1 ||
        source.arguments.front().kind != sexpr::AtomKind::symbol || source.children.empty() ||
        !sexpr::is_head(source.children.front(), "scene")) {
        add_error(diagnostics, source.source, "invalid_animation_definition",
                  "animation requires a name and begins with a scene form");
        return false;
    }
    const sexpr::Form& scene_form = source.children.front();
    const Symbol* scene_name = symbol_argument(scene_form, 0, diagnostics);
    if (scene_name == nullptr || !exact_arguments(scene_form, 1, diagnostics)) return false;
    const auto scene_id = content::find(catalog.scenes, *scene_name);
    if (!scene_id) {
        add_error(diagnostics, scene_form.source, "unknown_animation_scene",
                  "unknown scene '" + scene_name->text + "'");
        return false;
    }
    const content::SceneDef* scene = content::get(catalog.scenes, *scene_id);
    if (scene == nullptr) return false;

    // Lower structured sequence and parallel blocks into a deterministic event timeline.
    TimelineCompiler compiler{
        .catalog = catalog,
        .scene = *scene,
        .program = {},
        .diagnostics = diagnostics,
    };
    compiler.program.scene = *scene_id;
    const std::vector<sexpr::Form> body(source.children.begin() + 1, source.children.end());
    compiler.program.duration = compile_sequence(body, 0, compiler);
    std::stable_sort(compiler.program.events.begin(), compiler.program.events.end(),
                     [](const content::AnimationEvent& left, const content::AnimationEvent& right) {
                         return left.at_tick < right.at_tick;
                     });
    if (!diagnostics.ok()) return false;
    result = std::move(compiler.program);
    return true;
}

bool start_animation(const content::AnimationProgram& program, const content::Catalog& catalog,
                     AnimationState& state, Diagnostics& diagnostics) {
    const content::SceneDef* scene = content::get(catalog.scenes, program.scene);
    if (scene == nullptr) {
        add_error(diagnostics, {}, "animation_scene_out_of_range",
                  "animation references a scene outside the active catalog");
        return false;
    }

    // Materialize mutable scene nodes; the catalog stays immutable and shareable.
    AnimationState started;
    started.program = &program;
    started.finished = false;
    started.nodes.reserve(scene->nodes.size());
    for (const content::SceneNodeDef& source : scene->nodes) {
        started.nodes.push_back({
            .name = source.name,
            .kind = source.kind,
            .x = static_cast<float>(source.x),
            .y = static_cast<float>(source.y),
            .layer = source.layer,
            .visible = source.visible,
            .space = content::CoordinateSpace::native_canvas,
        });
    }
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
        if (tween.node >= state.nodes.size()) continue;
        const float progress =
            static_cast<float>(state.tick - tween.begin_tick) / static_cast<float>(tween.duration);
        const float amount = eased(progress, tween.ease);
        AnimationNode& node = state.nodes[tween.node];
        node.x = tween.from_x + (tween.to_x - tween.from_x) * amount;
        node.y = tween.from_y + (tween.to_y - tween.from_y) * amount;
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

const AnimationNode* find_animation_node(const AnimationState& state, const Symbol& name) {
    const auto found = runtime_node_index(state, name);
    return found ? &state.nodes[*found] : nullptr;
}

} // namespace pokered
