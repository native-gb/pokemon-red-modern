#pragma once

namespace pokered {

struct CampaignProgramCatalog;
struct CampaignState;
struct RuleCatalog;
struct WorldState;

namespace render {

void draw_field_menu_overlay(
    const WorldState& world, const CampaignState& campaign,
    const CampaignProgramCatalog& programs,
    const RuleCatalog& rules);

} // namespace render
} // namespace pokered
