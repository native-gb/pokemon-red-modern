#include "predicates.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace pokered {
namespace {

enum class ValueKind {
    boolean,
    integer,
};

struct Compiler {
    const content::Catalog& catalog;
    content::PredicateProgram program;
    Diagnostics& diagnostics;
    std::uint32_t stack_depth{};
};

struct Value {
    ValueKind kind{ValueKind::integer};
    std::int64_t integer{};
};

void emit(Compiler& compiler, content::PredicateOp operation, std::int64_t operand,
          const SourceSpan& source, int stack_change) {
    compiler.program.instructions.push_back({
        .operation = operation,
        .operand = operand,
        .source = source,
    });

    // Track the verified maximum so evaluation never needs an unbounded operand stack.
    const std::int64_t next_depth = static_cast<std::int64_t>(compiler.stack_depth) + stack_change;
    compiler.stack_depth = static_cast<std::uint32_t>(std::max<std::int64_t>(next_depth, 0));
    compiler.program.maximum_stack = std::max(compiler.program.maximum_stack, compiler.stack_depth);
}

bool require_count(const sexpr::Form& form, std::size_t expected, Diagnostics& diagnostics) {
    const std::size_t actual = form.arguments.size() + form.children.size();
    if (actual == expected) return true;
    add_error(diagnostics, form.source, "predicate_argument_count",
              "'" + form.head.symbol.text + "' requires " + std::to_string(expected) +
                  " operand(s), but received " + std::to_string(actual));
    return false;
}

bool require_child_count(const sexpr::Form& form, std::size_t minimum, Diagnostics& diagnostics) {
    if (form.arguments.empty() && form.children.size() >= minimum) return true;
    add_error(diagnostics, form.source, "predicate_expression_shape",
              "'" + form.head.symbol.text + "' requires indented expression operands");
    return false;
}

const Symbol* symbol_argument(const sexpr::Form& form, Diagnostics& diagnostics) {
    if (form.arguments.size() == 1 && form.children.empty() &&
        form.arguments.front().kind == sexpr::AtomKind::symbol) {
        return &form.arguments.front().symbol;
    }
    add_error(diagnostics, form.source, "predicate_symbol_argument",
              "'" + form.head.symbol.text + "' requires one symbol argument");
    return nullptr;
}

template <class IdType, class Record>
std::optional<IdType> resolve(const sexpr::Form& form, const content::Index<IdType, Record>& index,
                              Diagnostics& diagnostics) {
    const Symbol* key = symbol_argument(form, diagnostics);
    if (key == nullptr) return std::nullopt;
    const std::optional<IdType> id = content::find(index, *key);
    if (id) return id;
    add_error(diagnostics, form.arguments.front().source, "unknown_predicate_reference",
              "unknown reference '" + key->text + "' used by '" + form.head.symbol.text + "'");
    return std::nullopt;
}

bool compile_expression(const sexpr::Form& form, Compiler& compiler, ValueKind& result);

bool compile_children(const sexpr::Form& form, Compiler& compiler, ValueKind required) {
    for (const sexpr::Form& child : form.children) {
        ValueKind kind = ValueKind::integer;
        if (!compile_expression(child, compiler, kind)) return false;
        if (kind == required) continue;
        add_error(compiler.diagnostics, child.source, "predicate_type_mismatch",
                  "expression has the wrong value type for '" + form.head.symbol.text + "'");
        return false;
    }
    return true;
}

bool compile_comparison(const sexpr::Form& form, Compiler& compiler, content::PredicateOp operation,
                        bool integers_only, ValueKind& result) {
    if (!require_child_count(form, 2, compiler.diagnostics) || form.children.size() != 2)
        return false;

    ValueKind left = ValueKind::integer;
    ValueKind right = ValueKind::integer;
    if (!compile_expression(form.children[0], compiler, left) ||
        !compile_expression(form.children[1], compiler, right)) {
        return false;
    }
    if (left != right || (integers_only && left != ValueKind::integer)) {
        add_error(compiler.diagnostics, form.source, "predicate_type_mismatch",
                  "comparison operands must have matching numeric types");
        return false;
    }
    emit(compiler, operation, 0, form.source, -1);
    result = ValueKind::boolean;
    return true;
}

bool compile_arithmetic(const sexpr::Form& form, Compiler& compiler, content::PredicateOp operation,
                        ValueKind& result) {
    if (!require_child_count(form, 2, compiler.diagnostics) || form.children.size() != 2)
        return false;
    if (!compile_children(form, compiler, ValueKind::integer)) return false;
    emit(compiler, operation, 0, form.source, -1);
    result = ValueKind::integer;
    return true;
}

bool compile_boolean_fold(const sexpr::Form& form, Compiler& compiler,
                          content::PredicateOp operation, ValueKind& result) {
    if (!require_child_count(form, 2, compiler.diagnostics)) return false;
    if (!compile_children(form, compiler, ValueKind::boolean)) return false;
    for (std::size_t index = 1; index < form.children.size(); ++index)
        emit(compiler, operation, 0, form.source, -1);
    result = ValueKind::boolean;
    return true;
}

bool compile_flag_query(const sexpr::Form& form, Compiler& compiler, ValueKind& result) {
    const auto flag = resolve(form, compiler.catalog.flags, compiler.diagnostics);
    if (!flag) return false;
    emit(compiler, content::PredicateOp::flag_set, flag->value, form.source, 1);
    result = ValueKind::boolean;
    return true;
}

bool compile_species_query(const sexpr::Form& form, Compiler& compiler,
                           content::PredicateOp operation, ValueKind& result) {
    const auto species = resolve(form, compiler.catalog.species, compiler.diagnostics);
    if (!species) return false;
    emit(compiler, operation, species->value, form.source, 1);
    result = ValueKind::boolean;
    return true;
}

bool compile_expression(const sexpr::Form& form, Compiler& compiler, ValueKind& result) {
    // Literal lines are leaf expressions in the indented notation.
    if (form.head.kind == sexpr::AtomKind::boolean) {
        emit(compiler, content::PredicateOp::push_boolean, form.head.boolean ? 1 : 0, form.source,
             1);
        result = ValueKind::boolean;
        return true;
    }
    if (form.head.kind == sexpr::AtomKind::integer) {
        emit(compiler, content::PredicateOp::push_integer, form.head.integer, form.source, 1);
        result = ValueKind::integer;
        return true;
    }
    if (form.head.kind != sexpr::AtomKind::symbol) {
        add_error(compiler.diagnostics, form.source, "unsupported_predicate_literal",
                  "predicates accept only boolean and integer literals");
        return false;
    }

    const std::string& operation = form.head.symbol.text;
    if (operation == "equal")
        return compile_comparison(form, compiler, content::PredicateOp::equal, false, result);
    if (operation == "not_equal")
        return compile_comparison(form, compiler, content::PredicateOp::not_equal, false, result);
    if (operation == "less")
        return compile_comparison(form, compiler, content::PredicateOp::less, true, result);
    if (operation == "less_or_equal")
        return compile_comparison(form, compiler, content::PredicateOp::less_or_equal, true,
                                  result);
    if (operation == "greater")
        return compile_comparison(form, compiler, content::PredicateOp::greater, true, result);
    if (operation == "greater_or_equal")
        return compile_comparison(form, compiler, content::PredicateOp::greater_or_equal, true,
                                  result);
    if (operation == "and")
        return compile_boolean_fold(form, compiler, content::PredicateOp::logical_and, result);
    if (operation == "or")
        return compile_boolean_fold(form, compiler, content::PredicateOp::logical_or, result);
    if (operation == "add")
        return compile_arithmetic(form, compiler, content::PredicateOp::add, result);
    if (operation == "subtract")
        return compile_arithmetic(form, compiler, content::PredicateOp::subtract, result);
    if (operation == "multiply")
        return compile_arithmetic(form, compiler, content::PredicateOp::multiply, result);
    if (operation == "divide")
        return compile_arithmetic(form, compiler, content::PredicateOp::divide, result);
    if (operation == "minimum")
        return compile_arithmetic(form, compiler, content::PredicateOp::minimum, result);
    if (operation == "maximum")
        return compile_arithmetic(form, compiler, content::PredicateOp::maximum, result);

    if (operation == "not") {
        if (!require_child_count(form, 1, compiler.diagnostics) || form.children.size() != 1 ||
            !compile_children(form, compiler, ValueKind::boolean)) {
            return false;
        }
        emit(compiler, content::PredicateOp::logical_not, 0, form.source, 0);
        result = ValueKind::boolean;
        return true;
    }
    if (operation == "flag_set" || operation == "badge_owned" || operation == "trainer_defeated" ||
        operation == "option_enabled") {
        return compile_flag_query(form, compiler, result);
    }
    if (operation == "variable") {
        const auto variable = resolve(form, compiler.catalog.variables, compiler.diagnostics);
        if (!variable) return false;
        emit(compiler, content::PredicateOp::variable, variable->value, form.source, 1);
        result = ValueKind::integer;
        return true;
    }
    if (operation == "has_item" || operation == "item_count") {
        const auto item = resolve(form, compiler.catalog.items, compiler.diagnostics);
        if (!item) return false;
        const content::PredicateOp opcode = operation == "has_item"
                                                ? content::PredicateOp::has_item
                                                : content::PredicateOp::item_count;
        emit(compiler, opcode, item->value, form.source, 1);
        result = operation == "has_item" ? ValueKind::boolean : ValueKind::integer;
        return true;
    }
    if (operation == "party_has_space") {
        if (!require_count(form, 0, compiler.diagnostics)) return false;
        emit(compiler, content::PredicateOp::party_has_space, 0, form.source, 1);
        result = ValueKind::boolean;
        return true;
    }
    if (operation == "party_contains")
        return compile_species_query(form, compiler, content::PredicateOp::party_contains, result);
    if (operation == "species_seen")
        return compile_species_query(form, compiler, content::PredicateOp::species_seen, result);
    if (operation == "species_owned")
        return compile_species_query(form, compiler, content::PredicateOp::species_owned, result);
    if (operation == "actor_visible") {
        const auto actor = resolve(form, compiler.catalog.actor_spawns, compiler.diagnostics);
        if (!actor) return false;
        emit(compiler, content::PredicateOp::actor_visible, actor->value, form.source, 1);
        result = ValueKind::boolean;
        return true;
    }
    if (operation == "current_map") {
        const auto map = resolve(form, compiler.catalog.maps, compiler.diagnostics);
        if (!map) return false;
        emit(compiler, content::PredicateOp::current_map, map->value, form.source, 1);
        result = ValueKind::boolean;
        return true;
    }
    if (operation == "player_level" || operation == "friendship" || operation == "battle_result") {
        if (!require_count(form, 0, compiler.diagnostics)) return false;
        content::PredicateOp opcode = content::PredicateOp::player_level;
        if (operation == "friendship") opcode = content::PredicateOp::friendship;
        if (operation == "battle_result") opcode = content::PredicateOp::battle_result;
        emit(compiler, opcode, 0, form.source, 1);
        result = ValueKind::integer;
        return true;
    }

    add_error(compiler.diagnostics, form.source, "unknown_predicate_operation",
              "unknown predicate operation '" + operation + "'");
    return false;
}

bool read_indexed_boolean(std::span<const std::uint8_t> values, std::int64_t operand,
                          bool& result) {
    if (operand < 0 || static_cast<std::uint64_t>(operand) >= values.size()) return false;
    result = values[static_cast<std::size_t>(operand)] != 0;
    return true;
}

template <class ValueType>
bool read_indexed(std::span<const ValueType> values, std::int64_t operand, ValueType& result) {
    if (operand < 0 || static_cast<std::uint64_t>(operand) >= values.size()) return false;
    result = values[static_cast<std::size_t>(operand)];
    return true;
}

bool pop(std::vector<Value>& stack, ValueKind kind, Value& result) {
    if (stack.empty() || stack.back().kind != kind) return false;
    result = stack.back();
    stack.pop_back();
    return true;
}

bool binary(std::vector<Value>& stack, ValueKind kind, Value& left, Value& right) {
    return pop(stack, kind, right) && pop(stack, kind, left);
}

bool execute_operator(const content::PredicateInstruction& instruction, std::vector<Value>& stack,
                      Diagnostics& diagnostics) {
    Value left;
    Value right;
    const auto push_boolean = [&stack](bool value) {
        stack.push_back({.kind = ValueKind::boolean, .integer = value ? 1 : 0});
    };

    // Comparison and arithmetic opcodes consume verified stack values.
    switch (instruction.operation) {
    case content::PredicateOp::equal:
    case content::PredicateOp::not_equal: {
        if (stack.size() < 2 || stack[stack.size() - 1].kind != stack[stack.size() - 2].kind) break;
        right = stack.back();
        stack.pop_back();
        left = stack.back();
        stack.pop_back();
        push_boolean(instruction.operation == content::PredicateOp::equal
                         ? left.integer == right.integer
                         : left.integer != right.integer);
        return true;
    }
    case content::PredicateOp::less:
    case content::PredicateOp::less_or_equal:
    case content::PredicateOp::greater:
    case content::PredicateOp::greater_or_equal:
        if (!binary(stack, ValueKind::integer, left, right)) break;
        if (instruction.operation == content::PredicateOp::less)
            push_boolean(left.integer < right.integer);
        else if (instruction.operation == content::PredicateOp::less_or_equal)
            push_boolean(left.integer <= right.integer);
        else if (instruction.operation == content::PredicateOp::greater)
            push_boolean(left.integer > right.integer);
        else
            push_boolean(left.integer >= right.integer);
        return true;
    case content::PredicateOp::logical_and:
    case content::PredicateOp::logical_or:
        if (!binary(stack, ValueKind::boolean, left, right)) break;
        push_boolean(instruction.operation == content::PredicateOp::logical_and
                         ? left.integer != 0 && right.integer != 0
                         : left.integer != 0 || right.integer != 0);
        return true;
    case content::PredicateOp::logical_not:
        if (!pop(stack, ValueKind::boolean, left)) break;
        push_boolean(left.integer == 0);
        return true;
    case content::PredicateOp::add:
    case content::PredicateOp::subtract:
    case content::PredicateOp::multiply:
    case content::PredicateOp::divide:
    case content::PredicateOp::minimum:
    case content::PredicateOp::maximum:
        if (!binary(stack, ValueKind::integer, left, right)) break;
        if (instruction.operation == content::PredicateOp::divide && right.integer == 0) {
            add_error(diagnostics, instruction.source, "predicate_divide_by_zero",
                      "predicate attempted integer division by zero");
            return false;
        }
        if (instruction.operation == content::PredicateOp::add)
            left.integer += right.integer;
        else if (instruction.operation == content::PredicateOp::subtract)
            left.integer -= right.integer;
        else if (instruction.operation == content::PredicateOp::multiply)
            left.integer *= right.integer;
        else if (instruction.operation == content::PredicateOp::divide)
            left.integer /= right.integer;
        else if (instruction.operation == content::PredicateOp::minimum)
            left.integer = std::min(left.integer, right.integer);
        else
            left.integer = std::max(left.integer, right.integer);
        stack.push_back(left);
        return true;
    default:
        return false;
    }

    add_error(diagnostics, instruction.source, "predicate_stack_corruption",
              "compiled predicate encountered an invalid operand stack");
    return false;
}

} // namespace

bool compile_predicate(const sexpr::Form& source, const content::Catalog& catalog,
                       content::PredicateProgram& result, Diagnostics& diagnostics) {
    if (!sexpr::is_head(source, "predicate") || source.arguments.size() != 1 ||
        source.arguments.front().kind != sexpr::AtomKind::symbol || source.children.size() != 1) {
        add_error(diagnostics, source.source, "invalid_predicate_definition",
                  "predicate requires a name and exactly one expression");
        return false;
    }

    // Compile postorder typed instructions, then require a boolean root value.
    Compiler compiler{.catalog = catalog, .program = {}, .diagnostics = diagnostics};
    ValueKind kind = ValueKind::integer;
    if (!compile_expression(source.children.front(), compiler, kind)) return false;
    if (kind != ValueKind::boolean) {
        add_error(diagnostics, source.children.front().source, "predicate_not_boolean",
                  "a predicate root expression must produce a boolean");
        return false;
    }
    result = std::move(compiler.program);
    return diagnostics.ok();
}

bool evaluate_predicate(const content::PredicateProgram& program, const PredicateState& state,
                        bool& result, Diagnostics& diagnostics) {
    std::vector<Value> stack;
    stack.reserve(program.maximum_stack);

    // Execute simple pushes and state queries directly; operators use one checked helper.
    for (const content::PredicateInstruction& instruction : program.instructions) {
        bool boolean = false;
        std::int64_t integer = 0;
        switch (instruction.operation) {
        case content::PredicateOp::push_integer:
            stack.push_back({.kind = ValueKind::integer, .integer = instruction.operand});
            continue;
        case content::PredicateOp::push_boolean:
            stack.push_back({.kind = ValueKind::boolean, .integer = instruction.operand});
            continue;
        case content::PredicateOp::flag_set:
            if (read_indexed_boolean(state.flags, instruction.operand, boolean)) {
                stack.push_back({.kind = ValueKind::boolean, .integer = boolean ? 1 : 0});
                continue;
            }
            break;
        case content::PredicateOp::variable:
            if (read_indexed(state.variables, instruction.operand, integer)) {
                stack.push_back({.kind = ValueKind::integer, .integer = integer});
                continue;
            }
            break;
        case content::PredicateOp::has_item:
        case content::PredicateOp::item_count: {
            std::uint32_t count = 0;
            if (!read_indexed(state.item_counts, instruction.operand, count)) break;
            const bool has_item = instruction.operation == content::PredicateOp::has_item;
            stack.push_back({.kind = has_item ? ValueKind::boolean : ValueKind::integer,
                             .integer = has_item ? (count != 0 ? 1 : 0) : count});
            continue;
        }
        case content::PredicateOp::party_has_space:
            stack.push_back({.kind = ValueKind::boolean,
                             .integer = state.party.size() < state.party_capacity ? 1 : 0});
            continue;
        case content::PredicateOp::party_contains: {
            const auto species =
                content::SpeciesId{static_cast<std::uint32_t>(instruction.operand)};
            const bool found =
                std::find(state.party.begin(), state.party.end(), species) != state.party.end();
            stack.push_back({.kind = ValueKind::boolean, .integer = found ? 1 : 0});
            continue;
        }
        case content::PredicateOp::species_seen:
        case content::PredicateOp::species_owned:
        case content::PredicateOp::actor_visible: {
            std::span<const std::uint8_t> values = state.species_seen;
            if (instruction.operation == content::PredicateOp::species_owned)
                values = state.species_owned;
            if (instruction.operation == content::PredicateOp::actor_visible)
                values = state.actors_visible;
            if (read_indexed_boolean(values, instruction.operand, boolean)) {
                stack.push_back({.kind = ValueKind::boolean, .integer = boolean ? 1 : 0});
                continue;
            }
            break;
        }
        case content::PredicateOp::current_map:
            stack.push_back({
                .kind = ValueKind::boolean,
                .integer =
                    state.current_map.value == static_cast<std::uint32_t>(instruction.operand) ? 1
                                                                                               : 0,
            });
            continue;
        case content::PredicateOp::player_level:
            stack.push_back({.kind = ValueKind::integer, .integer = state.player_level});
            continue;
        case content::PredicateOp::friendship:
            stack.push_back({.kind = ValueKind::integer, .integer = state.friendship});
            continue;
        case content::PredicateOp::battle_result:
            stack.push_back({.kind = ValueKind::integer,
                             .integer = static_cast<std::int64_t>(state.battle_result)});
            continue;
        default:
            if (execute_operator(instruction, stack, diagnostics)) continue;
            return false;
        }

        add_error(diagnostics, instruction.source, "predicate_state_out_of_range",
                  "predicate referenced state outside the active catalog");
        return false;
    }

    // Publication validation guarantees one final boolean, but keep corrupted packs safe.
    if (stack.size() != 1 || stack.back().kind != ValueKind::boolean) {
        add_error(diagnostics, {}, "predicate_invalid_result",
                  "predicate did not finish with exactly one boolean value");
        return false;
    }
    result = stack.back().integer != 0;
    return true;
}

} // namespace pokered
