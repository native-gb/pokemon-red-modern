#include "field_menu.hpp"

namespace pokered {
namespace {

constexpr std::size_t root_entry_count = 5U;

} // namespace

void open_field_menu(FieldMenuState& menu) {
    menu = {
        .page = FieldMenuPage::root,
        .selected = 0U,
        .input_cooldown = 0U,
        .save_requested = false,
        .open = true,
    };
}

void step_field_menu(FieldMenuState& menu,
                     const FieldMenuInput& input) {
    if (!menu.open) return;
    if (input.start) {
        menu = {};
        return;
    }
    if (input.back) {
        if (menu.page == FieldMenuPage::root)
            menu = {};
        else {
            menu.page = FieldMenuPage::root;
            menu.selected = 0U;
        }
        return;
    }
    if (menu.page != FieldMenuPage::root) return;
    if (menu.input_cooldown > 0U)
        --menu.input_cooldown;
    else if (input.up || input.down) {
        if (input.up)
            menu.selected =
                menu.selected == 0U ? root_entry_count - 1U
                                    : menu.selected - 1U;
        else
            menu.selected =
                (menu.selected + 1U) % root_entry_count;
        menu.input_cooldown = 8U;
    }
    if (!input.confirm) return;
    switch (menu.selected) {
    case 0U:
        menu.page = FieldMenuPage::party;
        break;
    case 1U:
        menu.page = FieldMenuPage::bag;
        break;
    case 2U:
        menu.page = FieldMenuPage::trainer;
        break;
    case 3U:
        menu.save_requested = true;
        menu.open = false;
        break;
    default:
        menu = {};
        break;
    }
}

} // namespace pokered
