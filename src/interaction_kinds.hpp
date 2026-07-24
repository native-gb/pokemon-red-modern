#pragma once

#include <cstdint>

namespace pokered {

// These are semantic interaction families decoded by the cartridge importer.
// Map, actor, text, item, and shop payloads remain imported content.
enum class InteractionBuiltin : std::uint8_t {
    none,
    pokecenter_nurse,
    bills_pc,
    players_pc,
    pokecenter_pc,
    prize_vendor,
    cable_club_receptionist,
    vending_machine,
    shop,
};

} // namespace pokered
