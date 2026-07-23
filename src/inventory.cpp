#include "inventory.hpp"

#include <algorithm>
#include <limits>

namespace pokered {

bool give_inventory_item(InventoryState& inventory, std::uint16_t item_id,
                         std::uint16_t quantity) {
    if (item_id == 0U || quantity == 0U) return false;
    const auto found = std::ranges::find_if(
        inventory.stacks, [item_id](const InventoryStack& stack) {
            return stack.item_id == item_id;
        });
    if (found == inventory.stacks.end()) {
        if (inventory.stack_capacity != 0U &&
            inventory.stacks.size() >=
                inventory.stack_capacity)
            return false;
        inventory.stacks.push_back({item_id, quantity});
        return true;
    }
    if (quantity >
        std::numeric_limits<std::uint16_t>::max() - found->quantity)
        return false;
    found->quantity =
        static_cast<std::uint16_t>(found->quantity + quantity);
    return true;
}

bool take_inventory_item(InventoryState& inventory, std::uint16_t item_id,
                         std::uint16_t quantity) {
    if (item_id == 0U || quantity == 0U) return false;
    const auto found = std::ranges::find_if(
        inventory.stacks, [item_id](const InventoryStack& stack) {
            return stack.item_id == item_id;
        });
    if (found == inventory.stacks.end() ||
        found->quantity < quantity)
        return false;
    found->quantity =
        static_cast<std::uint16_t>(found->quantity - quantity);
    if (found->quantity == 0U) inventory.stacks.erase(found);
    return true;
}

std::uint16_t inventory_item_quantity(const InventoryState& inventory,
                                      std::uint16_t item_id) {
    const auto found = std::ranges::find_if(
        inventory.stacks, [item_id](const InventoryStack& stack) {
            return stack.item_id == item_id;
        });
    return found == inventory.stacks.end() ? 0U : found->quantity;
}

} // namespace pokered
