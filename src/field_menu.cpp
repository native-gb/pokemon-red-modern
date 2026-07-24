#include "field_menu.hpp"

namespace pokered {
namespace {

constexpr std::size_t root_entry_count = 6U;

} // namespace

void open_field_menu(FieldMenuState& menu) {
    menu = {
        .page = FieldMenuPage::root,
        .selected = 0U,
        .input_cooldown = 0U,
        .save_requested = false,
        .quit_to_title_requested = false,
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
    const bool navigable =
        menu.page == FieldMenuPage::root ||
        menu.page == FieldMenuPage::confirm_quit;
    if (navigable && menu.input_cooldown > 0U)
        --menu.input_cooldown;
    else if (navigable && (input.up || input.down)) {
        const std::size_t entry_count =
            menu.page == FieldMenuPage::confirm_quit
                ? 2U
                : root_entry_count;
        if (input.up)
            menu.selected =
                menu.selected == 0U ? entry_count - 1U
                                    : menu.selected - 1U;
        else
            menu.selected =
                (menu.selected + 1U) % entry_count;
        menu.input_cooldown = 8U;
    }
    if (!input.confirm) return;
    if (menu.page == FieldMenuPage::confirm_quit) {
        if (menu.selected == 0U) {
            menu.quit_to_title_requested = true;
            menu.open = false;
        } else {
            menu.page = FieldMenuPage::root;
            menu.selected = 4U;
        }
        return;
    }
    if (menu.page != FieldMenuPage::root) return;
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
    case 4U:
        menu.page = FieldMenuPage::confirm_quit;
        menu.selected = 1U;
        break;
    default:
        menu = {};
        break;
    }
}

} // namespace pokered
