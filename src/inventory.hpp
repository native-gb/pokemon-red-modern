#pragma once

#include <cstdint>
#include <vector>

namespace pokered {

struct InventoryStack {
    std::uint16_t item_id{};
    std::uint16_t quantity{};
};

struct InventoryState {
    std::vector<InventoryStack> stacks;
    std::uint16_t stack_capacity{};
};

bool give_inventory_item(InventoryState& inventory, std::uint16_t item_id,
                         std::uint16_t quantity);
bool take_inventory_item(InventoryState& inventory, std::uint16_t item_id,
                         std::uint16_t quantity);
std::uint16_t inventory_item_quantity(const InventoryState& inventory,
                                      std::uint16_t item_id);

} // namespace pokered
