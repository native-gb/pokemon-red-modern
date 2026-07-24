#pragma once

struct SDL_Renderer;

namespace pokered {

struct CampaignProgramCatalog;
struct CampaignState;
struct RuleCatalog;
struct WorldState;

namespace render {

struct BootRenderResources;

bool draw_field_menu_overlay(
    SDL_Renderer* renderer, int output_width, int output_height,
    const BootRenderResources& resources,
    const WorldState& world, const CampaignState& campaign,
    const CampaignProgramCatalog& programs,
    const RuleCatalog& rules);

} // namespace render
} // namespace pokered
