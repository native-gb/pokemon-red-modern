#include "import_campaign_programs.hpp"

#include "import_text.hpp"
#include "naming.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

constexpr std::size_t kPalletDefaultScriptOffset = 0x018E81U;
constexpr std::size_t kPalletOakAppearedOffset = 0x018EA7U;
constexpr std::size_t kPalletHeyWaitTextOffset = 0x018FB0U;
constexpr std::size_t kPalletUnsafeTextOffset = 0x018FCEU;
constexpr std::size_t kPalletOakPathOffset = 0x01A4DCU;
constexpr std::size_t kPalletPlayerPathOffset = 0x01A4E9U;
constexpr std::size_t kOakLabEntryPathOffset = 0x01CB7EU;
constexpr std::size_t kOakLabPlayerPathOffset = 0x01CBCFU;
constexpr std::array<std::size_t, 4> kOakLabChoiceTextOffsets{0x01D34FU, 0x01D35EU, 0x01D36DU,
                                                              0x01D37CU};
constexpr std::array<std::size_t, 3> kStarterPromptTextOffsets{
    0x01D19AU, 0x01D1A4U, 0x01D1AEU};
constexpr std::size_t kStarterEnergeticTextOffset = 0x01D222U;
constexpr std::size_t kPlayerReceivedStarterTextOffset = 0x094EA0U;
constexpr std::size_t kRivalTakesStarterTextOffset = 0x095444U;
constexpr std::size_t kRivalReceivedStarterTextOffset = 0x095461U;
constexpr std::size_t kRivalChallengeTextOffset = 0x095477U;
constexpr std::size_t kRivalPlayerVictoryTextOffset = 0x0954B6U;
constexpr std::size_t kRivalPlayerLossTextOffset = 0x0954E4U;
constexpr std::size_t kRivalExitTextOffset = 0x095502U;
constexpr std::size_t kNicknameQuestionTextOffset = 0x006557U;
constexpr std::size_t kLowercaseAlphabetOffset = 0x00679EU;
constexpr std::size_t kUppercaseAlphabetOffset = 0x0067D6U;
constexpr std::size_t kNicknameLabelOffset = 0x006953U;
constexpr std::size_t kPokemonNameLengthOffset = 0x0066DFU;
constexpr std::array<std::size_t, 3> kStarterChoiceCodeOffsets{
    0x01D102U, 0x01D113U, 0x01D124U};
constexpr std::array<std::size_t, 3> kRivalStarterPathOffsets{
    0x01CC9CU, 0x01CCB7U, 0x01CCEFU};
constexpr std::size_t kRivalBattleCodeOffset = 0x01CDB9U;
constexpr std::size_t kRivalExitPathOffset = 0x01CE66U;
constexpr std::size_t kViridianMartParcelCheckOffset = 0x01D47DU;
constexpr std::size_t kViridianMartPlayerPathOffset = 0x01D4BBU;
constexpr std::size_t kViridianMartParcelGrantOffset = 0x01D4CFU;
constexpr std::size_t kViridianMartParcelFlagOffset = 0x01D4D5U;
constexpr std::size_t kViridianMartCameFromPalletTextOffset = 0x01D4F5U;
constexpr std::size_t kViridianMartParcelQuestTextOffset = 0x01D4FAU;
constexpr std::size_t kToggleableObjectStatesOffset = 0x00CAEAU;
constexpr std::size_t kOakLabParcelItemCheckOffset = 0x01D2A9U;
constexpr std::size_t kOakLabRemoveParcelOffset = 0x01D00AU;
constexpr std::size_t kOakLabRivalMovementOffset = 0x01D02BU;
constexpr std::size_t kOakLabPokedexToggleOffset = 0x01CF54U;
constexpr std::size_t kOakLabProgressFlagsOffset = 0x01CF87U;
constexpr std::size_t kOakLabOldManToggleOffset = 0x01CF91U;
constexpr std::size_t kOakLabRouteFlagsOffset = 0x01CFE7U;
constexpr std::size_t kOakLabRouteRivalToggleOffset = 0x01CFF0U;
constexpr std::size_t kOakLabDeliverParcelTextOffset = 0x01D2FFU;
constexpr std::size_t kOakLabRivalGrampsTextOffset = 0x01D3D7U;
constexpr std::size_t kOakLabRivalWhatDidYouCallTextOffset = 0x01D3DCU;
constexpr std::size_t kOakLabRequestTextOffset = 0x01D3E1U;
constexpr std::size_t kOakLabPokedexInventionTextOffset = 0x01D3E6U;
constexpr std::size_t kOakLabGotPokedexTextOffset = 0x01D3EBU;
constexpr std::size_t kOakLabDreamTextOffset = 0x01D3F1U;
constexpr std::size_t kOakLabRivalLeaveTextOffset = 0x01D3F6U;
constexpr std::size_t kRoute22WantsBattleCheckOffset = 0x050F00U;
constexpr std::size_t kRoute22BattleCoordsOffset = 0x050F2DU;
constexpr std::size_t kRoute22ApproachPathOffset = 0x050EFBU;
constexpr std::size_t kRoute22TrainerSelectorOffset = 0x050F9EU;
constexpr std::size_t kRoute22TrainerTableOffset = 0x050FAFU;
constexpr std::size_t kRoute22BeatRivalFlagOffset = 0x050FD7U;
constexpr std::size_t kRoute22ExitPathUpperOffset = 0x05101FU;
constexpr std::size_t kRoute22ExitPathLowerOffset = 0x051017U;
constexpr std::size_t kRoute22HideRivalOffset = 0x051034U;
constexpr std::size_t kRoute22ClearFlagsOffset = 0x051041U;
constexpr std::size_t kRoute22BeforeBattleTextOffset = 0x0511ADU;
constexpr std::size_t kRoute22AfterBattleTextOffset = 0x0511B2U;
constexpr std::size_t kRoute22DefeatedTextOffset = 0x0511B7U;
constexpr std::size_t kRoute22VictoryTextOffset = 0x0511BCU;
constexpr std::size_t kBagInventoryCapacityOffset = 0x00CE19U;
constexpr std::size_t kItemNamesOffset = 0x00472BU;
constexpr std::size_t kFoundItemTextOffset = 0x080119U;
constexpr std::size_t kNoItemRoomTextOffset = 0x08012AU;
constexpr std::uint16_t kOrdinaryItemCount = 83U;
constexpr std::uint16_t kFirstHiddenMachine = 0xC4U;
constexpr std::uint16_t kHiddenMachineCount = 5U;
constexpr std::uint16_t kFirstTechnicalMachine = 0xC9U;
constexpr std::uint16_t kTechnicalMachineCount = 50U;
constexpr std::size_t kOakLabRoute22BeatCheckOffset = 0x01D280U;
constexpr std::size_t kOakLabPokeballGrantFlagOffset = 0x01D2D0U;
constexpr std::size_t kOakLabPokeballGrantOffset = 0x01D2D9U;
constexpr std::size_t kOakLabGivePokeballsTextOffset = 0x01D30EU;
constexpr std::size_t kOakLabComeSeeMeTextOffset = 0x01D318U;
constexpr std::size_t kBluesHouseDaisyChecksOffset = 0x019B5EU;
constexpr std::size_t kBluesHouseTownMapGrantOffset = 0x019B7AU;
constexpr std::size_t kBluesHouseTownMapToggleOffset = 0x019B82U;
constexpr std::size_t kBluesHouseTownMapFlagOffset = 0x019B92U;
constexpr std::size_t kBluesHouseRivalAtLabTextOffset = 0x019BAAU;
constexpr std::size_t kBluesHouseOfferMapTextOffset = 0x019BAFU;
constexpr std::size_t kBluesHouseGotMapTextOffset = 0x019BB4U;
constexpr std::size_t kBluesHouseBagFullTextOffset = 0x019BBAU;
constexpr std::size_t kBluesHouseUseMapTextOffset = 0x019BBFU;
constexpr std::size_t kBluesHouseEnteredFlagOffset = 0x019B4BU;
constexpr std::size_t kPalletGotPokeballsCheckOffset = 0x018E5BU;
constexpr std::size_t kPalletAfterPokeballsFlagOffset = 0x018E62U;
constexpr std::size_t kPalletDaisyWalkingCheckOffset = 0x018F56U;
constexpr std::size_t kPalletDaisyWalkingFlagOffset = 0x018F63U;
constexpr std::size_t kPalletDaisySittingToggleOffset = 0x018F68U;
constexpr std::size_t kPalletDaisyWalkingToggleOffset = 0x018F72U;
constexpr std::size_t kPalletAfterPokeballs2FlagOffset = 0x018F82U;
constexpr std::size_t kRoute1PotionProgramOffset = 0x01CAB9U;
constexpr std::size_t kRoute1PotionGrantOffset = 0x01CAC8U;
constexpr std::size_t kRoute1MartSampleTextOffset = 0x01CAE3U;
constexpr std::size_t kRoute1GotPotionTextOffset = 0x01CAE8U;
constexpr std::size_t kRoute1RepeatTextOffset = 0x01CAEEU;
constexpr std::size_t kRoute1NoRoomTextOffset = 0x01CAF3U;
constexpr std::size_t kPewterGymBrockPostBattleOffset = 0x05C3D2U;
constexpr std::size_t kPewterGymBeatBrockSetOffset = 0x05C3E6U;
constexpr std::size_t kPewterGymTmGrantOffset = 0x05C3EBU;
constexpr std::size_t kPewterGymGotTmSetOffset = 0x05C3FAU;
constexpr std::size_t kPewterGymBagFullOffset = 0x05C401U;
constexpr std::size_t kPewterGymObtainedBadgeSetOffset = 0x05C408U;
constexpr std::size_t kPewterGymBeatGymSetOffset = 0x05C40DU;
constexpr std::size_t kPewterGymHideGymGuideOffset = 0x05C412U;
constexpr std::size_t kPewterGymHideRouteRivalOffset = 0x05C41CU;
constexpr std::size_t kPewterGymClearRouteFlagsOffset = 0x05C426U;
constexpr std::size_t kPewterGymTrainerDefeatedSetOffset = 0x05C42DU;
constexpr std::size_t kPewterGymBrockActorOffset = 0x05C53AU;
constexpr std::size_t kPewterGymBrockCheckOffset = 0x05C44FU;
constexpr std::size_t kPewterGymGotTmCheckOffset = 0x05C456U;
constexpr std::size_t kPewterGymBrockPreBattleTextOffset = 0x05C49EU;
constexpr std::size_t kPewterGymBrockAdviceTextOffset = 0x05C4A3U;
constexpr std::size_t kPewterGymWaitTakeThisTextOffset = 0x05C4A8U;
constexpr std::size_t kPewterGymReceivedTmTextOffset = 0x05C4ADU;
constexpr std::size_t kPewterGymTmNoRoomTextOffset = 0x05C4B7U;
constexpr std::size_t kPewterGymBadgeTextOffset = 0x05C4BCU;
constexpr std::size_t kPewterGymGuidePreAdviceTextOffset = 0x05C515U;
constexpr std::size_t kPewterGymGuideBeginAdviceTextOffset = 0x05C51AU;
constexpr std::size_t kPewterGymGuideAdviceTextOffset = 0x05C51FU;
constexpr std::size_t kPewterGymGuideFreeServiceTextOffset = 0x05C524U;
constexpr std::size_t kPewterGymGuidePostBattleTextOffset = 0x05C529U;
constexpr std::size_t kPewterCityObjectOffset = 0x018577U;
constexpr std::size_t kPewterCityEastGateCoordsOffset = 0x019277U;
constexpr std::size_t kPewterCityMuseumQuestionTextOffset = 0x0193F1U;
constexpr std::size_t kPewterCityMuseumYesTextOffset = 0x0193F6U;
constexpr std::size_t kPewterCityMuseumNoTextOffset = 0x0193FBU;
constexpr std::size_t kPewterCityMuseumArrivalTextOffset = 0x019400U;
constexpr std::size_t kPewterCityRepelQuestionTextOffset = 0x019427U;
constexpr std::size_t kPewterCityRepelYesTextOffset = 0x01942CU;
constexpr std::size_t kPewterCityRepelNoTextOffset = 0x019431U;
constexpr std::size_t kPewterCityGymFollowTextOffset = 0x01945DU;
constexpr std::size_t kPewterCityGymArrivalTextOffset = 0x019462U;
constexpr std::size_t kMtMoonB2FDefaultScriptOffset = 0x049D6FU;
constexpr std::size_t kMtMoonB2FFossilAreaCoordsOffset = 0x049D37U;
constexpr std::size_t kMtMoonB2FBeatSuperNerdSetOffset = 0x049DA8U;
constexpr std::size_t kMtMoonB2FSuperNerdActorOffset = 0x049FEFU;
constexpr std::size_t kMtMoonB2FDomeGrantOffset = 0x049EFEU;
constexpr std::size_t kMtMoonB2FHelixGrantOffset = 0x049F3EU;
constexpr std::size_t kMtMoonB2FDomeToggleOffset = 0x049F0AU;
constexpr std::size_t kMtMoonB2FHelixToggleOffset = 0x049F4AU;
constexpr std::size_t kMtMoonB2FDomeSetOffset = 0x049F14U;
constexpr std::size_t kMtMoonB2FHelixSetOffset = 0x049F54U;
constexpr std::size_t kMtMoonB2FDomeQuestionTextOffset = 0x049F24U;
constexpr std::size_t kMtMoonB2FHelixQuestionTextOffset = 0x049F64U;
constexpr std::size_t kMtMoonB2FReceivedTextOffset = 0x049F6FU;
constexpr std::size_t kMtMoonB2FNoRoomTextOffset = 0x049F7FU;
constexpr std::size_t kMtMoonB2FBothMineTextOffset = 0x049F85U;
constexpr std::size_t kMtMoonB2FOkShareTextOffset = 0x049F8AU;
constexpr std::size_t kMtMoonB2FEachTakeOneTextOffset = 0x049F8FU;
constexpr std::size_t kMtMoonB2FPokemonLabTextOffset = 0x049F94U;
constexpr std::size_t kMtMoonB2FThenMineTextOffset = 0x049F99U;
constexpr std::size_t kMtMoonPokecenterObjectOffset = 0x049376U;
constexpr std::size_t kMtMoonMagikarpEventCheckOffset = 0x0492EDU;
constexpr std::size_t kMtMoonMagikarpPriceOffset = 0x04930CU;
constexpr std::size_t kMtMoonMagikarpGrantOffset = 0x04931EU;
constexpr std::size_t kMtMoonMagikarpEventSetOffset = 0x049347U;
constexpr std::size_t kMtMoonMagikarpOfferTextOffset = 0x04935CU;
constexpr std::size_t kMtMoonMagikarpNoTextOffset = 0x049361U;
constexpr std::size_t kMtMoonMagikarpNoMoneyTextOffset = 0x049366U;
constexpr std::size_t kMtMoonMagikarpNoRefundsTextOffset = 0x04936BU;
constexpr std::size_t kGotPokemonTextOffset = 0x04FE39U;
constexpr std::size_t kSentPokemonToBoxTextOffset = 0x04FE3FU;
constexpr std::size_t kPokemonBoxFullTextOffset = 0x04FE44U;
constexpr std::size_t kCeruleanCityObjectOffset = 0x018786U;
constexpr std::size_t kCeruleanRivalEventCheckOffset = 0x0194F7U;
constexpr std::size_t kCeruleanRivalTriggerCoordsOffset = 0x019554U;
constexpr std::size_t kCeruleanRivalApproachPathOffset = 0x019559U;
constexpr std::size_t kCeruleanRivalSelectorOffset = 0x019588U;
constexpr std::size_t kCeruleanRivalEventSetOffset = 0x0195C1U;
constexpr std::size_t kCeruleanRivalExitLeftOffset = 0x019600U;
constexpr std::size_t kCeruleanRivalExitRightOffset = 0x019608U;
constexpr std::size_t kCeruleanRivalPreBattleTextOffset = 0x019668U;
constexpr std::size_t kCeruleanRivalDefeatedTextOffset = 0x01966DU;
constexpr std::size_t kCeruleanRivalVictoryTextOffset = 0x019672U;
constexpr std::size_t kCeruleanRivalAfterBattleTextOffset = 0x019677U;
constexpr std::size_t kInitialMoneyCodeOffset = 0x00F880U;
constexpr std::size_t kGivePokemonPartyCapacityOffset = 0x04FDB0U;
constexpr std::size_t kGivePokemonBoxCapacityOffset = 0x04FDB7U;
constexpr std::size_t kStorageBoxCountOffset = 0x0739C1U;
constexpr std::size_t kPokedexOrderOffset = 0x041024U;
constexpr std::size_t kInternalSpeciesCount = 190U;
constexpr std::uint8_t kTrainerOpponentOffset = 0xC8U;
constexpr std::uint8_t kViridianMartMapId = 42U;
constexpr std::uint32_t kFollowedOakFlag = static_cast<std::uint32_t>(0xD747U * 8U);
constexpr std::uint32_t kFollowedOakSecondFlag = kFollowedOakFlag + 32U;
constexpr std::uint32_t kOakAskedToChooseFlag = kFollowedOakFlag + 33U;
constexpr std::uint32_t kGotStarterFlag = kFollowedOakFlag + 34U;
constexpr std::uint32_t kBattledLabRivalFlag =
    kFollowedOakFlag + 35U;
constexpr std::uint32_t kOakAppearedFlag = static_cast<std::uint32_t>(0xD74BU * 8U + 7U);
constexpr std::uint8_t kPlayerStarterVariable = 0U;
constexpr std::uint8_t kRivalStarterVariable = 1U;

enum class TriggerKind : std::uint8_t {
    player_y,
    actor_activation,
    map_entry,
    player_rectangle,
    map_presence,
};

enum class Opcode : std::uint8_t {
    lock_input,
    set_flag,
    clear_flag,
    show_actor,
    hide_actor,
    say,
    face_actor,
    face_player,
    move_actor_to_player,
    align_pair_x,
    parallel_path,
    ask_yes_no,
    end_if_choice_no,
    set_variable,
    give_pokemon,
    try_give_pokemon,
    jump_if_pokemon_grant_failed,
    say_if_pokemon_sent_to_box,
    jump_if_money_below,
    take_money,
    nickname_last_party_member_if_yes,
    player_path,
    give_item,
    try_give_item,
    take_item,
    place_actor,
    place_actor_at_player_x,
    actor_path,
    jump_if_player_y,
    jump_if_item_grant_failed,
    jump,
    wait_ticks,
    actor_path_by_player_x,
    actor_path_by_player_y,
    start_trainer_battle,
    jump_if_choice_no,
    say_if_player_won,
    say_if_player_lost,
    jump_if_player_won,
    end_if_player_lost,
    heal_party,
    escort_player_to,
    unlock_input,
    end,
};

enum class PathCommand : std::uint8_t {
    down,
    up,
    left,
    right,
    wait,
    face_down,
};

struct Instruction {
    Opcode opcode{Opcode::end};
    std::uint8_t a{};
    std::uint8_t b{};
    std::uint32_t value{};
    std::vector<std::string> pages;
    std::vector<PathCommand> actor_path;
    std::vector<PathCommand> player_path;
};

struct Program {
    std::string key;
    TriggerKind trigger_kind{TriggerKind::player_y};
    std::uint8_t trigger_map{};
    std::uint8_t trigger_x{};
    std::uint8_t trigger_y{};
    std::uint8_t trigger_width{};
    std::uint8_t trigger_height{};
    std::uint32_t required_flag{0xFFFFFFFFU};
    std::uint32_t absent_flag{0xFFFFFFFFU};
    std::uint16_t required_variable{0xFFFFU};
    std::uint16_t required_variable_value{};
    std::uint16_t required_item_id{};
    std::uint16_t required_item_quantity{};
    std::vector<std::pair<std::uint8_t, std::uint8_t>> initially_hidden;
    std::vector<Instruction> instructions;
};

struct StarterChoice {
    std::uint8_t player_species{};
    std::uint8_t rival_species{};
    std::uint8_t selected_ball{};
    std::uint8_t rival_ball{};
};

struct RivalBattleChoice {
    std::uint8_t rival_species{};
    std::uint8_t trainer_class{};
    std::uint16_t trainer_party{};
};

struct ParcelProgram {
    std::uint32_t oak_got_parcel_flag{};
    std::uint32_t got_oaks_parcel_flag{};
    std::uint16_t item_id{};
    std::uint8_t quantity{};
    std::vector<PathCommand> player_path;
    DecodedTextProgram came_from_pallet;
    DecodedTextProgram parcel_quest;
};

struct ToggleActor {
    std::uint8_t map_id{};
    std::uint8_t actor_index{};
    bool initially_visible{};
};

struct OakReturnProgram {
    std::uint16_t parcel_item_id{};
    std::uint8_t parcel_quantity{};
    std::uint32_t got_pokedex_flag{};
    std::uint32_t oak_got_parcel_flag{};
    std::uint32_t first_route_22_rival_flag{};
    std::uint32_t second_route_22_rival_flag{};
    std::uint32_t route_22_rival_wants_battle_flag{};
    ToggleActor pokedex_1;
    ToggleActor pokedex_2;
    ToggleActor lying_old_man;
    ToggleActor standing_old_man;
    ToggleActor route_22_rival;
    std::array<std::uint8_t, 3> player_y{3U, 1U, 2U};
    std::array<std::uint8_t, 3> movement_steps{};
    std::array<std::uint8_t, 3> spawn_y{};
    std::uint8_t spawn_x{};
    DecodedTextProgram deliver_parcel;
    DecodedTextProgram rival_gramps;
    DecodedTextProgram rival_what_did_you_call;
    DecodedTextProgram oak_request;
    DecodedTextProgram pokedex_invention;
    DecodedTextProgram got_pokedex;
    DecodedTextProgram oak_dream;
    DecodedTextProgram rival_leave;
};

struct Route22FirstRivalProgram {
    std::uint8_t map_id{};
    std::uint8_t actor_index{};
    std::uint8_t trigger_x{};
    std::uint8_t trigger_y{};
    std::uint8_t trigger_width{};
    std::uint8_t trigger_height{};
    std::uint32_t first_rival_flag{};
    std::uint32_t wants_battle_flag{};
    std::uint32_t beat_rival_flag{};
    std::array<RivalBattleChoice, 3> battles;
    std::vector<PathCommand> approach_upper;
    std::vector<PathCommand> approach_lower;
    std::vector<PathCommand> exit_upper;
    std::vector<PathCommand> exit_lower;
    DecodedTextProgram before_battle;
    DecodedTextProgram defeated;
    DecodedTextProgram victory;
    DecodedTextProgram after_battle;
};

struct OakPokeballProgram {
    std::uint32_t beat_route_22_rival_flag{};
    std::uint32_t got_pokeballs_flag{};
    std::uint16_t item_id{};
    std::uint8_t quantity{};
    DecodedTextProgram give_pokeballs;
    DecodedTextProgram come_see_me;
};

struct DaisyTownMapProgram {
    std::uint32_t got_pokedex_flag{};
    std::uint32_t got_town_map_flag{};
    std::uint16_t item_id{};
    std::uint8_t quantity{};
    ToggleActor town_map_actor;
    std::string item_name;
    DecodedTextProgram rival_at_lab;
    DecodedTextProgram offer_map;
    DecodedTextProgram got_map;
    DecodedTextProgram bag_full;
    DecodedTextProgram use_map;
};

struct PalletRewardUpdates {
    std::uint32_t entered_blues_house_flag{};
    std::uint32_t got_pokeballs_flag{};
    std::uint32_t pallet_after_pokeballs_flag{};
    std::uint32_t got_town_map_flag{};
    std::uint32_t daisy_walking_flag{};
    std::uint32_t pallet_after_pokeballs_2_flag{};
    ToggleActor daisy_sitting;
    ToggleActor daisy_walking;
};

struct Route1PotionProgram {
    std::uint32_t got_potion_sample_flag{};
    std::uint16_t item_id{};
    std::uint8_t quantity{};
    std::string item_name;
    DecodedTextProgram sample;
    DecodedTextProgram got_item;
    DecodedTextProgram repeat;
    DecodedTextProgram no_room;
};

struct PewterGymProgram {
    std::uint8_t map_id{};
    std::uint8_t brock_actor_index{};
    std::uint8_t guide_actor_index{};
    std::uint8_t trainer_class{};
    std::uint16_t trainer_party{};
    std::uint32_t beat_brock_flag{};
    std::uint32_t got_tm_flag{};
    std::uint32_t obtained_badge_flag{};
    std::uint32_t beat_gym_flag{};
    std::uint32_t gym_trainer_defeated_flag{};
    std::uint32_t first_route_rival_flag{};
    std::uint32_t route_rival_wants_battle_flag{};
    std::uint16_t tm_item_id{};
    std::uint8_t tm_quantity{};
    std::string tm_name;
    ToggleActor gym_guide;
    ToggleActor route_rival;
    DecodedTextProgram pre_battle;
    DecodedTextProgram badge;
    DecodedTextProgram wait_take_this;
    DecodedTextProgram received_tm;
    DecodedTextProgram no_room;
    DecodedTextProgram post_battle_advice;
    DecodedTextProgram guide_pre_advice;
    DecodedTextProgram guide_begin_advice;
    DecodedTextProgram guide_advice;
    DecodedTextProgram guide_free_service;
    DecodedTextProgram guide_post_battle;
};

struct PewterCityProgram {
    struct Position {
        std::uint8_t x{};
        std::uint8_t y{};
    };

    std::uint8_t map_id{};
    std::uint8_t museum_actor_index{};
    std::uint8_t repel_actor_index{};
    std::uint8_t gym_guide_actor_index{};
    std::uint32_t beat_brock_flag{};
    Position museum_actor_origin;
    Position repel_actor_origin;
    Position gym_guide_actor_origin;
    Position museum_player_target;
    Position gym_player_target;
    std::array<Position, 4> east_gate_cells;
    DecodedTextProgram museum_question;
    DecodedTextProgram museum_yes;
    DecodedTextProgram museum_no;
    DecodedTextProgram museum_arrival;
    DecodedTextProgram repel_question;
    DecodedTextProgram repel_yes;
    DecodedTextProgram repel_no;
    DecodedTextProgram gym_follow;
    DecodedTextProgram gym_arrival;
};

struct MtMoonFossilProgram {
    std::uint8_t map_id{};
    std::uint8_t super_nerd_actor_index{};
    std::uint8_t dome_actor_index{};
    std::uint8_t helix_actor_index{};
    std::uint8_t trainer_class{};
    std::uint16_t trainer_party{};
    std::uint8_t trigger_x{};
    std::uint8_t trigger_y{};
    std::uint8_t suppression_x{};
    std::uint8_t suppression_y{};
    std::uint8_t suppression_width{};
    std::uint8_t suppression_height{};
    std::uint32_t beat_super_nerd_flag{};
    std::uint32_t got_dome_flag{};
    std::uint32_t got_helix_flag{};
    std::uint16_t dome_item_id{};
    std::uint16_t helix_item_id{};
    std::uint8_t dome_quantity{};
    std::uint8_t helix_quantity{};
    std::string dome_item_name;
    std::string helix_item_name;
    ToggleActor dome_actor;
    ToggleActor helix_actor;
    DecodedTextProgram both_mine;
    DecodedTextProgram okay_share;
    DecodedTextProgram each_take_one;
    DecodedTextProgram pokemon_lab;
    DecodedTextProgram dome_question;
    DecodedTextProgram helix_question;
    DecodedTextProgram dome_received;
    DecodedTextProgram helix_received;
    DecodedTextProgram no_room;
    DecodedTextProgram then_mine;
};

struct MtMoonMagikarpProgram {
    std::uint8_t map_id{};
    std::uint8_t actor_index{};
    std::uint32_t bought_flag{};
    std::uint32_t price{};
    std::uint8_t species_dex{};
    std::uint8_t level{};
    DecodedTextProgram offer;
    DecodedTextProgram chose_no;
    DecodedTextProgram no_money;
    DecodedTextProgram no_refunds;
    DecodedTextProgram got_pokemon;
    DecodedTextProgram sent_to_box;
    DecodedTextProgram box_full;
};

struct CeruleanRivalProgram {
    std::uint8_t map_id{};
    std::uint8_t actor_index{};
    std::uint8_t trigger_x{};
    std::uint8_t trigger_y{};
    std::uint8_t trigger_width{};
    std::uint8_t approach_start_y{};
    std::uint32_t beat_flag{};
    std::array<RivalBattleChoice, 3> battles;
    std::vector<PathCommand> approach;
    std::vector<PathCommand> exit_left;
    std::vector<PathCommand> exit_right;
    DecodedTextProgram pre_battle;
    DecodedTextProgram defeated;
    DecodedTextProgram victory;
    DecodedTextProgram after_battle;
};

struct CampaignInitialState {
    std::uint32_t money{};
    std::uint8_t party_capacity{};
    std::uint8_t storage_box_count{};
    std::uint16_t storage_box_capacity{};
};

Instruction operation(Opcode opcode, std::uint8_t a = 0U, std::uint8_t b = 0U,
                      std::uint32_t value = 0U) {
    Instruction result;
    result.opcode = opcode;
    result.a = a;
    result.b = b;
    result.value = value;
    return result;
}

Instruction dialogue(std::vector<std::string> pages) {
    Instruction result = operation(Opcode::say);
    result.pages = std::move(pages);
    return result;
}

Instruction species_dialogue(std::vector<std::string> pages,
                             std::uint8_t species_dex) {
    Instruction result = dialogue(std::move(pages));
    result.value = species_dex;
    return result;
}

Instruction ask_yes_no(std::vector<std::string> pages,
                       std::uint8_t species_dex) {
    Instruction result = operation(Opcode::ask_yes_no, 0U, 0U,
                                   species_dex);
    result.pages = std::move(pages);
    return result;
}

bool has_bytes(std::span<const std::uint8_t> rom, std::size_t offset,
               std::span<const std::uint8_t> expected) {
    return offset <= rom.size() && expected.size() <= rom.size() - offset &&
           std::equal(expected.begin(), expected.end(),
                      rom.begin() + static_cast<std::ptrdiff_t>(offset));
}

bool decode_checked_event(std::span<const std::uint8_t> rom,
                          std::size_t offset, std::uint32_t& flag,
                          std::string& error) {
    if (offset > rom.size() || 5U > rom.size() - offset ||
        rom[offset] != 0xFAU || rom[offset + 3U] != 0xCBU ||
        rom[offset + 4U] < 0x47U ||
        (rom[offset + 4U] - 0x47U) % 8U != 0U) {
        error = "campaign event check does not match the verified ROM";
        return false;
    }
    const std::uint8_t bit =
        static_cast<std::uint8_t>((rom[offset + 4U] - 0x47U) / 8U);
    if (bit > 7U) {
        error = "campaign event check has an invalid bit";
        return false;
    }
    const std::uint16_t address = static_cast<std::uint16_t>(
        rom[offset + 1U] |
        static_cast<std::uint16_t>(rom[offset + 2U]) << 8U);
    flag = static_cast<std::uint32_t>(address) * 8U + bit;
    return true;
}

bool decode_set_event(std::span<const std::uint8_t> rom,
                      std::size_t offset, std::uint32_t& flag,
                      std::string& error) {
    if (offset > rom.size() || 5U > rom.size() - offset ||
        rom[offset] != 0x21U || rom[offset + 3U] != 0xCBU ||
        rom[offset + 4U] < 0xC6U ||
        (rom[offset + 4U] - 0xC6U) % 8U != 0U) {
        error = "campaign event mutation does not match the verified ROM";
        return false;
    }
    const std::uint8_t bit =
        static_cast<std::uint8_t>((rom[offset + 4U] - 0xC6U) / 8U);
    if (bit > 7U) {
        error = "campaign event mutation has an invalid bit";
        return false;
    }
    const std::uint16_t address = static_cast<std::uint16_t>(
        rom[offset + 1U] |
        static_cast<std::uint16_t>(rom[offset + 2U]) << 8U);
    flag = static_cast<std::uint32_t>(address) * 8U + bit;
    return true;
}

bool decode_reused_event(std::span<const std::uint8_t> rom,
                         std::size_t offset, std::uint16_t address,
                         bool set, std::uint32_t& flag,
                         std::string& error) {
    const std::uint8_t base = set ? 0xC6U : 0x86U;
    if (offset > rom.size() || 2U > rom.size() - offset ||
        rom[offset] != 0xCBU || rom[offset + 1U] < base ||
        (rom[offset + 1U] - base) % 8U != 0U) {
        error = "campaign reused event mutation does not match the verified ROM";
        return false;
    }
    const std::uint8_t bit =
        static_cast<std::uint8_t>((rom[offset + 1U] - base) / 8U);
    if (bit > 7U) {
        error = "campaign reused event mutation has an invalid bit";
        return false;
    }
    flag = static_cast<std::uint32_t>(address) * 8U + bit;
    return true;
}

bool decode_toggle_actors(
    std::span<const std::uint8_t> rom,
    std::vector<ToggleActor>& actors, std::string& error) {
    actors.clear();
    std::size_t cursor = kToggleableObjectStatesOffset;
    for (std::size_t index = 0U; index < 512U; ++index) {
        if (cursor >= rom.size()) {
            error =
                "toggleable actor table extends outside the verified ROM";
            return false;
        }
        const std::uint8_t map_id = rom[cursor++];
        if (map_id == 0xFFU) return !actors.empty();
        if (cursor + 1U >= rom.size()) {
            error = "toggleable actor record is truncated";
            return false;
        }
        const std::uint8_t actor_index = rom[cursor++];
        const std::uint8_t state = rom[cursor++];
        if (actor_index == 0U ||
            (state != 0x11U && state != 0x15U)) {
            error =
                "toggleable actor record does not match the verified layout";
            return false;
        }
        actors.push_back({
            .map_id = map_id,
            .actor_index = actor_index,
            .initially_visible = state == 0x15U,
        });
    }
    error = "toggleable actor table is missing its terminator";
    return false;
}

bool decode_naming_string(std::span<const std::uint8_t> rom,
                          std::size_t offset, std::size_t limit,
                          std::string& result) {
    result.clear();
    for (std::size_t cursor = offset;
         cursor < rom.size() && cursor - offset < limit; ++cursor) {
        if (rom[cursor] == 0x50U) return !result.empty();
        result += decode_text_glyph(rom[cursor]);
    }
    return false;
}

bool decode_inventory_stack_capacity(
    std::span<const std::uint8_t> rom,
    std::uint16_t& result, std::string& error) {
    constexpr std::array<std::uint8_t, 5> signature{
        0xD3U, 0xBCU, 0x20U, 0x02U, 0x16U};
    if (kBagInventoryCapacityOffset < signature.size() ||
        kBagInventoryCapacityOffset >= rom.size() ||
        !has_bytes(
            rom,
            kBagInventoryCapacityOffset - signature.size(),
            signature) ||
        rom[kBagInventoryCapacityOffset] == 0U) {
        error =
            "bag inventory capacity does not match the verified ROM";
        return false;
    }
    result = rom[kBagInventoryCapacityOffset];
    return true;
}

bool decode_campaign_initial_state(
    std::span<const std::uint8_t> rom,
    CampaignInitialState& result, std::string& error) {
    constexpr std::array<std::uint8_t, 7> money_signature{
        0x3EU, 0x30U, 0x32U, 0xAFU, 0x22U, 0x23U, 0x77U};
    if (!has_bytes(
            rom, kInitialMoneyCodeOffset, money_signature) ||
        kGivePokemonPartyCapacityOffset == 0U ||
        kGivePokemonPartyCapacityOffset >= rom.size() ||
        kGivePokemonBoxCapacityOffset == 0U ||
        kGivePokemonBoxCapacityOffset >= rom.size() ||
        kStorageBoxCountOffset == 0U ||
        kStorageBoxCountOffset >= rom.size() ||
        rom[kGivePokemonPartyCapacityOffset - 1U] != 0xFEU ||
        rom[kGivePokemonBoxCapacityOffset - 1U] != 0xFEU ||
        rom[kStorageBoxCountOffset - 1U] != 0x3EU) {
        error =
            "campaign initial economy and storage state does not match the verified ROM";
        return false;
    }
    const auto bcd_byte = [](std::uint8_t value) {
        return static_cast<std::uint32_t>(
            (value >> 4U) * 10U + (value & 0x0FU));
    };
    const std::uint8_t middle_money =
        rom[kInitialMoneyCodeOffset + 1U];
    if ((middle_money & 0x0FU) > 9U ||
        (middle_money >> 4U) > 9U) {
        error =
            "campaign initial money contains invalid packed BCD";
        return false;
    }
    result = {
        .money = bcd_byte(middle_money) * 100U,
        .party_capacity =
            rom[kGivePokemonPartyCapacityOffset],
        .storage_box_count = rom[kStorageBoxCountOffset],
        .storage_box_capacity =
            rom[kGivePokemonBoxCapacityOffset],
    };
    if (result.money == 0U || result.party_capacity == 0U ||
        result.storage_box_count == 0U ||
        result.storage_box_capacity == 0U) {
        error =
            "campaign initial economy and storage state is empty";
        return false;
    }
    return true;
}

bool decode_item_name(std::span<const std::uint8_t> rom,
                      std::uint16_t item_id,
                      std::string& result,
                      std::string& error) {
    if (item_id == 0U) {
        error = "item name lookup has an invalid item ID";
        return false;
    }
    std::size_t cursor = kItemNamesOffset;
    for (std::uint16_t current = 1U; current <= item_id;
         ++current) {
        result.clear();
        bool terminated = false;
        for (std::size_t length = 0U; length < 64U; ++length) {
            if (cursor >= rom.size()) {
                error = "item name table extends outside the verified ROM";
                return false;
            }
            const std::uint8_t encoded = rom[cursor++];
            if (encoded == 0x50U) {
                terminated = true;
                break;
            }
            result += decode_text_glyph(encoded);
        }
        if (!terminated || result.empty()) {
            error =
                "item name table contains an invalid entry";
            return false;
        }
    }
    return true;
}

struct ImportedItemName {
    std::uint16_t item_id{};
    std::string name;
};

std::string machine_name(std::string_view prefix, std::uint16_t number) {
    std::string result(prefix);
    result.push_back(static_cast<char>('0' + number / 10U));
    result.push_back(static_cast<char>('0' + number % 10U));
    return result;
}

bool decode_item_names(
    std::span<const std::uint8_t> rom,
    std::vector<ImportedItemName>& result, std::string& error) {
    result.clear();
    result.reserve(
        kOrdinaryItemCount + kHiddenMachineCount +
        kTechnicalMachineCount);
    for (std::uint16_t item_id = 1U; item_id <= kOrdinaryItemCount;
         ++item_id) {
        std::string name;
        if (!decode_item_name(rom, item_id, name, error))
            return false;
        result.push_back({item_id, std::move(name)});
    }
    for (std::uint16_t index = 0U; index < kHiddenMachineCount; ++index)
        result.push_back({
            static_cast<std::uint16_t>(kFirstHiddenMachine + index),
            machine_name("HM", static_cast<std::uint16_t>(index + 1U)),
        });
    for (std::uint16_t index = 0U; index < kTechnicalMachineCount; ++index)
        result.push_back({
            static_cast<std::uint16_t>(kFirstTechnicalMachine + index),
            machine_name("TM", static_cast<std::uint16_t>(index + 1U)),
        });
    return true;
}

void replace_all(std::string& text, std::string_view token,
                 std::string_view value) {
    std::size_t position = 0U;
    while ((position = text.find(token, position)) != std::string::npos) {
        text.replace(position, token.size(), value);
        position += value.size();
    }
}

bool decode_map_actor_owner(
    std::span<const std::uint8_t> rom,
    std::size_t object_offset, std::uint8_t text_id,
    std::uint8_t& actor_index, std::string& error) {
    actor_index = 0U;
    if (object_offset + 2U > rom.size()) {
        error = "map object header is truncated";
        return false;
    }
    std::size_t cursor = object_offset + 1U;
    const std::uint8_t warp_count = rom[cursor++];
    if (static_cast<std::size_t>(warp_count) >
        (rom.size() - cursor) / 4U) {
        error = "map warp table is truncated";
        return false;
    }
    cursor += static_cast<std::size_t>(warp_count) * 4U;
    if (cursor >= rom.size()) {
        error = "map background-event header is truncated";
        return false;
    }
    const std::uint8_t background_count = rom[cursor++];
    if (static_cast<std::size_t>(background_count) >
        (rom.size() - cursor) / 3U) {
        error = "map background-event table is truncated";
        return false;
    }
    cursor +=
        static_cast<std::size_t>(background_count) * 3U;
    if (cursor >= rom.size()) {
        error = "map actor header is truncated";
        return false;
    }
    const std::uint8_t actor_count = rom[cursor++];
    for (std::uint8_t actor = 1U;
         actor <= actor_count; ++actor) {
        if (cursor + 6U > rom.size()) {
            error = "map actor table is truncated";
            return false;
        }
        const std::uint8_t text_flags = rom[cursor + 5U];
        if ((text_flags & 0x3FU) == text_id) {
            if (actor_index != 0U) {
                error = "map actor owner is ambiguous";
                return false;
            }
            actor_index = actor;
        }
        cursor +=
            (text_flags & 0xC0U) == 0U ? 6U : 8U;
    }
    if (actor_index == 0U) {
        error = "map actor owner is missing";
        return false;
    }
    return true;
}

bool decode_loose_item_presentation(
    std::span<const std::uint8_t> rom,
    DecodedTextProgram& found, DecodedTextProgram& no_room,
    std::string& error) {
    constexpr std::array<std::uint8_t, 5> line_then_item_slot{
        0x4FU, 0x50U, 0x01U, 0x4BU, 0xCFU};
    if (!decode_text_program(
            rom, 0x20U, kFoundItemTextOffset, found) ||
        !found.complete || found.pages.empty() ||
        !decode_text_program(
            rom, 0x20U, kNoItemRoomTextOffset, no_room) ||
        !no_room.complete || no_room.pages.empty() ||
        !has_bytes(
            rom, kFoundItemTextOffset + 8U,
            line_then_item_slot)) {
        error =
            "loose-item presentation could not be decoded from the pinned ROM";
        return false;
    }
    bool owns_item_slot = false;
    for (std::string& page : found.pages) {
        const std::size_t item_slot = page.find("{ram_cf4b}");
        if (item_slot != std::string::npos) {
            owns_item_slot = true;
            // The source string ends with LINE immediately before the
            // separate text_ram command. Preserve that imported cursor
            // operation when lowering the transient string slot.
            page.insert(item_slot, "\n");
        }
        replace_all(page, "{ram_cf4b}", "{item_name}");
    }
    if (!owns_item_slot) {
        error =
            "loose-item presentation is missing its imported item-name slot";
        return false;
    }
    return true;
}

void resolve_item_name_buffer(
    DecodedTextProgram& text, std::string_view item_name) {
    for (std::string& page : text.pages) {
        const std::size_t position = page.find("{ram_");
        const std::size_t end =
            position == std::string::npos
                ? std::string::npos
                : page.find('}', position);
        if (end == std::string::npos) continue;
        std::string replacement{item_name};
        if (position != 0U && page[position - 1U] != '\n')
            replacement.insert(replacement.begin(), '\n');
        page.replace(position, end - position + 1U,
                     replacement);
    }
}

bool decode_naming_alphabet(
    std::span<const std::uint8_t> rom, std::size_t offset,
    std::array<std::string, kNamingCells>& cells,
    std::string& action, std::string& error) {
    constexpr std::size_t kSerializedAlphabetSize = 56U;
    if (offset > rom.size() ||
        kSerializedAlphabetSize > rom.size() - offset ||
        rom[offset + kNamingCells - 1U] != 0xF0U ||
        rom[offset + kSerializedAlphabetSize - 1U] != 0x50U) {
        error = "naming alphabet does not match the verified ROM layout";
        return false;
    }
    for (std::size_t index = 0U; index < kNamingCells; ++index)
        cells[index] =
            index + 1U == kNamingCells
                ? "END"
                : decode_text_glyph(rom[offset + index]);
    if (!decode_naming_string(rom, offset + kNamingCells,
                              kSerializedAlphabetSize - kNamingCells,
                              action)) {
        error = "naming alphabet action is unterminated";
        return false;
    }
    return true;
}

bool decode_naming_profile(std::span<const std::uint8_t> rom,
                           NamingProfile& profile,
                           std::string& heading,
                           std::string& error) {
    constexpr std::array<std::uint8_t, 4> length_signature{
        0xFAU, 0xE9U, 0xCEU, 0xFEU};
    if (kPokemonNameLengthOffset < length_signature.size() ||
        !has_bytes(rom,
                   kPokemonNameLengthOffset - length_signature.size(),
                   length_signature) ||
        rom[kPokemonNameLengthOffset] == 0U ||
        !decode_naming_alphabet(
            rom, kUppercaseAlphabetOffset, profile.uppercase,
            profile.lowercase_action, error) ||
        !decode_naming_alphabet(
            rom, kLowercaseAlphabetOffset, profile.lowercase,
            profile.uppercase_action, error)) {
        if (error.empty())
            error =
                "Pokemon nickname length program does not match the verified ROM";
        return false;
    }
    profile.maximum_length = rom[kPokemonNameLengthOffset];

    std::string nickname_label;
    if (!decode_naming_string(rom, kNicknameLabelOffset, 32U,
                              nickname_label)) {
        error = "Pokemon nickname heading is unterminated";
        return false;
    }
    heading = "{name_buffer}\n" + nickname_label;
    if (!valid_naming_profile(profile)) {
        error = "decoded Pokemon naming profile is invalid";
        return false;
    }
    return true;
}

std::uint8_t dex_for_internal_species(
    std::span<const std::uint8_t> rom, std::uint8_t internal) {
    if (internal == 0U || internal > kInternalSpeciesCount ||
        kPokedexOrderOffset + internal > rom.size())
        return 0U;
    return rom[kPokedexOrderOffset +
               static_cast<std::size_t>(internal - 1U)];
}

bool decode_starter_choice(std::span<const std::uint8_t> rom,
                           std::size_t offset, StarterChoice& result,
                           std::string& error) {
    constexpr std::array<std::uint8_t, 12> signature{
        0x08U, 0x3EU, 0x00U, 0xEAU, 0x3DU, 0xCDU,
        0x3EU, 0x00U, 0xEAU, 0x3EU, 0xCDU, 0x3EU};
    if (offset > rom.size() || 17U > rom.size() - offset) {
        error = "starter choice program extends outside the verified ROM";
        return false;
    }
    for (std::size_t index = 0U; index < signature.size(); ++index)
        if ((index != 2U && index != 7U) &&
            rom[offset + index] != signature[index]) {
            error = "starter choice program signature does not match the pinned ROM";
            return false;
        }
    if (rom[offset + 13U] != 0x06U ||
        (rom[offset + 15U] != 0x18U &&
         rom[offset + 15U] != 0xEAU)) {
        error = "starter choice program has an unknown selection tail";
        return false;
    }

    result = {
        .player_species =
            dex_for_internal_species(rom, rom[offset + 12U]),
        .rival_species =
            dex_for_internal_species(rom, rom[offset + 2U]),
        .selected_ball = rom[offset + 14U],
        .rival_ball = rom[offset + 7U],
    };
    if (result.player_species == 0U || result.player_species > 151U ||
        result.rival_species == 0U || result.rival_species > 151U ||
        result.selected_ball == 0U || result.rival_ball == 0U) {
        error = "starter choice program resolves invalid species or actors";
        return false;
    }
    return true;
}

bool decode_rival_battle_choices(
    std::span<const std::uint8_t> rom,
    const std::array<StarterChoice, 3>& starters,
    std::array<RivalBattleChoice, 3>& result, std::string& error) {
    constexpr std::array<std::pair<std::size_t, std::uint8_t>, 20>
        signature{{
            {0U, 0xFAU},  {1U, 0x30U},  {2U, 0xD7U},
            {3U, 0xCBU},  {4U, 0x47U},  {5U, 0xC0U},
            {6U, 0x3EU},  {8U, 0xEAU},  {9U, 0x59U},
            {10U, 0xD0U}, {11U, 0xFAU}, {12U, 0x15U},
            {13U, 0xD7U}, {14U, 0xFEU}, {16U, 0x20U},
            {18U, 0x3EU}, {22U, 0xFEU}, {24U, 0x20U},
            {26U, 0x3EU}, {30U, 0x3EU},
        }};
    if (kRivalBattleCodeOffset > rom.size() ||
        33U > rom.size() - kRivalBattleCodeOffset) {
        error = "lab rival battle selector extends outside the verified ROM";
        return false;
    }
    for (const auto& [relative, expected] : signature)
        if (rom[kRivalBattleCodeOffset + relative] != expected) {
            error =
                "lab rival battle selector does not match the pinned ROM";
            return false;
        }

    const std::uint8_t opponent =
        rom[kRivalBattleCodeOffset + 7U];
    if (opponent <= kTrainerOpponentOffset) {
        error = "lab rival battle selector has an invalid opponent";
        return false;
    }
    const std::uint8_t trainer_class =
        static_cast<std::uint8_t>(opponent - kTrainerOpponentOffset);
    const std::uint8_t first_species = dex_for_internal_species(
        rom, rom[kRivalBattleCodeOffset + 15U]);
    const std::uint8_t second_species = dex_for_internal_species(
        rom, rom[kRivalBattleCodeOffset + 23U]);
    const std::array<std::uint8_t, 3> one_based_parties{
        rom[kRivalBattleCodeOffset + 19U],
        rom[kRivalBattleCodeOffset + 27U],
        rom[kRivalBattleCodeOffset + 31U],
    };
    if (first_species == 0U || second_species == 0U ||
        first_species == second_species ||
        std::ranges::any_of(one_based_parties,
                           [](std::uint8_t party) {
                               return party == 0U;
                           })) {
        error = "lab rival battle selector resolves invalid content";
        return false;
    }
    const auto count_rival_species = [&](std::uint8_t species) {
        return std::ranges::count_if(
            starters, [species](const StarterChoice& starter) {
                return starter.rival_species == species;
            });
    };
    if (count_rival_species(first_species) != 1 ||
        count_rival_species(second_species) != 1) {
        error =
            "lab rival battle selector does not cover the starter branches";
        return false;
    }

    for (std::size_t index = 0U; index < starters.size(); ++index) {
        const std::uint8_t rival_species =
            starters[index].rival_species;
        const std::size_t party =
            rival_species == first_species
                ? 0U
                : rival_species == second_species ? 1U : 2U;
        result[index] = {
            .rival_species = rival_species,
            .trainer_class = trainer_class,
            .trainer_party = static_cast<std::uint16_t>(
                one_based_parties[party] - 1U),
        };
    }
    return true;
}

void write_u16(std::vector<std::uint8_t>& bytes, std::size_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void write_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U)
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
}

void write_string(std::vector<std::uint8_t>& bytes, std::string_view value) {
    write_u16(bytes, value.size());
    bytes.insert(bytes.end(), value.begin(), value.end());
}

void write_pages(std::vector<std::uint8_t>& bytes, const std::vector<std::string>& pages) {
    write_u16(bytes, pages.size());
    for (const std::string& page : pages)
        write_string(bytes, page);
}

void write_naming_profile(std::vector<std::uint8_t>& bytes,
                          const NamingProfile& profile,
                          std::string_view heading) {
    for (const std::string& cell : profile.uppercase)
        write_string(bytes, cell);
    for (const std::string& cell : profile.lowercase)
        write_string(bytes, cell);
    write_string(bytes, profile.uppercase_action);
    write_string(bytes, profile.lowercase_action);
    bytes.push_back(profile.maximum_length);
    write_string(bytes, heading);
}

void write_path(std::vector<std::uint8_t>& bytes, const std::vector<PathCommand>& path) {
    write_u16(bytes, path.size());
    for (const PathCommand command : path)
        bytes.push_back(static_cast<std::uint8_t>(command));
}

void write_program(std::vector<std::uint8_t>& bytes,
                   const Program& program) {
    write_string(bytes, program.key);
    bytes.push_back(static_cast<std::uint8_t>(program.trigger_kind));
    bytes.push_back(program.trigger_map);
    bytes.push_back(program.trigger_x);
    bytes.push_back(program.trigger_y);
    bytes.push_back(program.trigger_width);
    bytes.push_back(program.trigger_height);
    write_u32(bytes, program.required_flag);
    write_u32(bytes, program.absent_flag);
    write_u16(bytes, program.required_variable);
    write_u16(bytes, program.required_variable_value);
    write_u16(bytes, program.required_item_id);
    write_u16(bytes, program.required_item_quantity);
    write_u16(bytes, program.initially_hidden.size());
    for (const auto& [map_id, actor_index] : program.initially_hidden) {
        bytes.push_back(map_id);
        bytes.push_back(actor_index);
    }
    write_u16(bytes, program.instructions.size());
    for (const Instruction& instruction : program.instructions) {
        bytes.push_back(static_cast<std::uint8_t>(instruction.opcode));
        bytes.push_back(instruction.a);
        bytes.push_back(instruction.b);
        write_u32(bytes, instruction.value);
        write_pages(bytes, instruction.pages);
        write_path(bytes, instruction.actor_path);
        write_path(bytes, instruction.player_path);
    }
}

bool decode_rle_path(std::span<const std::uint8_t> rom, std::size_t offset, bool joypad,
                     std::vector<PathCommand>& result, std::string& error) {
    result.clear();
    std::size_t cursor = offset;
    for (std::size_t records = 0U; records < 64U; ++records) {
        if (cursor >= rom.size()) {
            error = "campaign movement RLE extends outside the verified ROM";
            return false;
        }
        const std::uint8_t encoded = rom[cursor++];
        if (encoded == 0xFFU) return !result.empty();
        if (cursor >= rom.size() || rom[cursor] == 0U) {
            error = "campaign movement RLE has an invalid run";
            return false;
        }
        PathCommand command;
        if (joypad) {
            switch (encoded) {
            case 0x40U:
                command = PathCommand::up;
                break;
            case 0x80U:
                command = PathCommand::down;
                break;
            case 0x20U:
                command = PathCommand::left;
                break;
            case 0x10U:
                command = PathCommand::right;
                break;
            default:
                error = "campaign joypad RLE contains an unknown command";
                return false;
            }
        } else
            switch (encoded) {
            case 0x00U:
                command = PathCommand::down;
                break;
            case 0x40U:
                command = PathCommand::up;
                break;
            case 0x80U:
                command = PathCommand::left;
                break;
            case 0xC0U:
                command = PathCommand::right;
                break;
            case 0xE0U:
                command = PathCommand::face_down;
                break;
            default:
                error = "campaign movement RLE contains an unknown command";
                return false;
            }
        const std::uint8_t count = rom[cursor++];
        result.insert(result.end(), count, command);
    }
    error = "campaign movement RLE is missing its terminator";
    return false;
}

bool decode_direct_npc_path(std::span<const std::uint8_t> rom, std::size_t offset,
                            std::vector<PathCommand>& result, std::string& error) {
    result.clear();
    for (std::size_t cursor = offset; cursor < rom.size() && result.size() < 1024U; ++cursor) {
        const std::uint8_t encoded = rom[cursor];
        if (encoded == 0xFFU) {
            if (!result.empty()) return true;
            break;
        }
        if (encoded == 0x00U)
            result.push_back(PathCommand::down);
        else if (encoded == 0x40U)
            result.push_back(PathCommand::up);
        else if (encoded == 0x80U)
            result.push_back(PathCommand::left);
        else if (encoded == 0xC0U)
            result.push_back(PathCommand::right);
        else if (encoded == 0xE0U)
            result.push_back(PathCommand::face_down);
        else {
            error = "campaign direct NPC path contains an unknown command";
            return false;
        }
    }
    error = "campaign direct NPC path is missing its terminator";
    return false;
}

bool decode_viridian_mart_parcel(std::span<const std::uint8_t> rom,
                                ParcelProgram& result,
                                std::string& error) {
    constexpr std::array<std::uint8_t, 2> check_tail{0x20U, 0x05U};
    constexpr std::array<std::uint8_t, 3> give_item_call{
        0xCDU, 0x2EU, 0x3EU};
    constexpr std::array<std::uint8_t, 15> parcel_wait{
        0xFAU, 0x38U, 0xCDU, 0xA7U, 0xC0U,
        0xCDU, 0xD7U, 0x3DU, 0x3EU, 0x05U,
        0xE0U, 0x8CU, 0xCDU, 0x20U, 0x29U};

    result = {};
    if (!decode_checked_event(rom, kViridianMartParcelCheckOffset,
                              result.oak_got_parcel_flag, error) ||
        !has_bytes(rom, kViridianMartParcelCheckOffset + 5U,
                   check_tail) ||
        !has_bytes(rom, kViridianMartParcelGrantOffset + 3U,
                   give_item_call) ||
        !has_bytes(rom, kViridianMartParcelGrantOffset - 15U,
                   parcel_wait) ||
        !decode_set_event(rom, kViridianMartParcelFlagOffset,
                          result.got_oaks_parcel_flag, error)) {
        if (error.empty())
            error =
                "Viridian Mart parcel program does not match the verified ROM";
        return false;
    }
    if (kViridianMartParcelGrantOffset + 2U >= rom.size() ||
        rom[kViridianMartParcelGrantOffset] != 0x01U) {
        error =
            "Viridian Mart parcel item grant does not match the verified ROM";
        return false;
    }
    result.quantity = rom[kViridianMartParcelGrantOffset + 1U];
    result.item_id = rom[kViridianMartParcelGrantOffset + 2U];
    if (result.quantity == 0U || result.item_id == 0U) {
        error = "Viridian Mart parcel item grant is empty";
        return false;
    }
    if (!decode_rle_path(rom, kViridianMartPlayerPathOffset, true,
                         result.player_path, error))
        return false;

    // StartSimulatingJoypadStates consumes this buffer from its final index.
    std::reverse(result.player_path.begin(), result.player_path.end());
    if (!decode_text_program(
            rom, 0x07U, kViridianMartCameFromPalletTextOffset,
            result.came_from_pallet) ||
        !decode_text_program(
            rom, 0x07U, kViridianMartParcelQuestTextOffset,
            result.parcel_quest) ||
        result.came_from_pallet.pages.empty() ||
        result.parcel_quest.pages.empty()) {
        error =
            "Viridian Mart parcel dialogue could not be decoded from the pinned ROM";
        return false;
    }
    return true;
}

bool decode_toggle_operation(
    std::span<const std::uint8_t> rom, std::size_t offset,
    std::uint8_t expected_action,
    const std::vector<ToggleActor>& toggle_actors,
    ToggleActor& result, std::string& error,
    bool tail_jump = false) {
    constexpr std::array<std::uint8_t, 3> store_toggle{
        0xEAU, 0x4DU, 0xCCU};
    if (offset > rom.size() || 10U > rom.size() - offset ||
        rom[offset] != 0x3EU ||
        !has_bytes(rom, offset + 2U, store_toggle) ||
        rom[offset + 5U] != 0x3EU ||
        rom[offset + 6U] != expected_action ||
        rom[offset + 7U] !=
            (tail_jump ? 0xC3U : 0xCDU) ||
        rom[offset + 8U] != 0x6DU ||
        rom[offset + 9U] != 0x3EU ||
        rom[offset + 1U] >= toggle_actors.size()) {
        error =
            "campaign toggle operation does not match the verified ROM";
        return false;
    }
    result = toggle_actors[rom[offset + 1U]];
    return true;
}

bool decode_oak_return_program(
    std::span<const std::uint8_t> rom,
    const ParcelProgram& parcel,
    const std::vector<ToggleActor>& toggle_actors,
    OakReturnProgram& result, std::string& error) {
    constexpr std::array<std::uint8_t, 5> item_check_tail{
        0xCDU, 0x93U, 0x34U, 0x20U, 0x08U};
    constexpr std::array<std::uint8_t, 11> remove_prefix{
        0x21U, 0x1EU, 0xD3U, 0x01U, 0x00U, 0x00U,
        0x2AU, 0xFEU, 0xFFU, 0xC8U, 0xFEU};
    constexpr std::array<std::uint8_t, 3> remove_index_store{
        0xEAU, 0x92U, 0xCFU};
    constexpr std::array<std::uint8_t, 3> remove_quantity_store{
        0xEAU, 0x96U, 0xCFU};

    result = {};
    if (kOakLabParcelItemCheckOffset + 6U > rom.size() ||
        rom[kOakLabParcelItemCheckOffset] != 0x06U ||
        !has_bytes(rom, kOakLabParcelItemCheckOffset + 2U,
                   item_check_tail) ||
        !has_bytes(rom, kOakLabRemoveParcelOffset, remove_prefix) ||
        kOakLabRemoveParcelOffset + 30U > rom.size() ||
        !has_bytes(rom, kOakLabRemoveParcelOffset + 22U,
                   remove_index_store) ||
        rom[kOakLabRemoveParcelOffset + 25U] != 0x3EU ||
        !has_bytes(rom, kOakLabRemoveParcelOffset + 27U,
                   remove_quantity_store)) {
        error =
            "Oak parcel inventory program does not match the verified ROM";
        return false;
    }
    result.parcel_item_id =
        rom[kOakLabParcelItemCheckOffset + 1U];
    result.parcel_quantity =
        rom[kOakLabRemoveParcelOffset + 26U];
    if (result.parcel_item_id !=
            rom[kOakLabRemoveParcelOffset + 11U] ||
        result.parcel_item_id != parcel.item_id ||
        result.parcel_quantity != parcel.quantity ||
        result.parcel_item_id == 0U ||
        result.parcel_quantity == 0U) {
        error =
            "Oak parcel inventory paths disagree on item content";
        return false;
    }

    constexpr std::array<std::uint8_t, 11> movement_prefix{
        0x3EU, 0x7CU, 0xE0U, 0xEBU, 0x3EU, 0x08U,
        0xE0U, 0xEEU, 0xFAU, 0x61U, 0xD3U};
    if (!has_bytes(rom, kOakLabRivalMovementOffset,
                   movement_prefix) ||
        kOakLabRivalMovementOffset + 49U > rom.size() ||
        rom[kOakLabRivalMovementOffset + 11U] != 0xFEU ||
        rom[kOakLabRivalMovementOffset + 15U] != 0x3EU ||
        rom[kOakLabRivalMovementOffset + 22U] != 0x06U ||
        rom[kOakLabRivalMovementOffset + 26U] != 0xFEU ||
        rom[kOakLabRivalMovementOffset + 30U] != 0x3EU ||
        rom[kOakLabRivalMovementOffset + 37U] != 0x06U ||
        rom[kOakLabRivalMovementOffset + 41U] != 0x3EU ||
        rom[kOakLabRivalMovementOffset + 46U] != 0x06U) {
        error =
            "Oak request rival movement does not match the verified ROM";
        return false;
    }
    constexpr std::uint8_t coordinate_padding = 4U;
    const std::uint8_t encoded_x =
        rom[kOakLabRivalMovementOffset + 5U];
    const std::array<std::uint8_t, 3> encoded_y{
        rom[kOakLabRivalMovementOffset + 23U],
        rom[kOakLabRivalMovementOffset + 38U],
        rom[kOakLabRivalMovementOffset + 47U],
    };
    if (encoded_x < coordinate_padding ||
        std::ranges::any_of(
            encoded_y, [](std::uint8_t value) {
                return value < coordinate_padding;
            })) {
        error = "Oak request rival placement cannot be normalized";
        return false;
    }
    result.player_y[0] =
        rom[kOakLabRivalMovementOffset + 12U];
    result.player_y[1] =
        rom[kOakLabRivalMovementOffset + 27U];
    result.movement_steps = {
        rom[kOakLabRivalMovementOffset + 16U],
        rom[kOakLabRivalMovementOffset + 31U],
        rom[kOakLabRivalMovementOffset + 42U],
    };
    result.spawn_x =
        static_cast<std::uint8_t>(encoded_x - coordinate_padding);
    for (std::size_t index = 0U; index < encoded_y.size(); ++index)
        result.spawn_y[index] = static_cast<std::uint8_t>(
            encoded_y[index] - coordinate_padding);
    if (result.player_y[0] == result.player_y[1] ||
        std::ranges::any_of(
            result.movement_steps,
            [](std::uint8_t value) { return value == 0U; })) {
        error = "Oak request rival movement has invalid branches";
        return false;
    }

    if (!decode_set_event(
            rom, kOakLabProgressFlagsOffset,
            result.got_pokedex_flag, error) ||
        !decode_set_event(
            rom, kOakLabProgressFlagsOffset + 5U,
            result.oak_got_parcel_flag, error) ||
        result.oak_got_parcel_flag !=
            parcel.oak_got_parcel_flag ||
        !decode_set_event(
            rom, kOakLabRouteFlagsOffset,
            result.first_route_22_rival_flag, error)) {
        if (error.empty())
            error =
                "Oak request progression flags do not match the verified ROM";
        return false;
    }
    const std::uint16_t route_flag_address =
        static_cast<std::uint16_t>(
            rom[kOakLabRouteFlagsOffset + 1U] |
            static_cast<std::uint16_t>(
                rom[kOakLabRouteFlagsOffset + 2U])
                << 8U);
    if (!decode_reused_event(
            rom, kOakLabRouteFlagsOffset + 5U,
            route_flag_address, false,
            result.second_route_22_rival_flag, error) ||
        !decode_reused_event(
            rom, kOakLabRouteFlagsOffset + 7U,
            route_flag_address, true,
            result.route_22_rival_wants_battle_flag, error))
        return false;

    if (!decode_toggle_operation(
            rom, kOakLabPokedexToggleOffset, 0x11U,
            toggle_actors, result.pokedex_1, error) ||
        !decode_toggle_operation(
            rom, kOakLabPokedexToggleOffset + 10U, 0x11U,
            toggle_actors, result.pokedex_2, error) ||
        !decode_toggle_operation(
            rom, kOakLabOldManToggleOffset, 0x11U,
            toggle_actors, result.lying_old_man, error) ||
        !decode_toggle_operation(
            rom, kOakLabOldManToggleOffset + 10U, 0x15U,
            toggle_actors, result.standing_old_man, error) ||
        !decode_toggle_operation(
            rom, kOakLabRouteRivalToggleOffset, 0x15U,
            toggle_actors, result.route_22_rival, error))
        return false;

    const std::array<std::pair<std::size_t, DecodedTextProgram*>, 8>
        text_programs{{
            {kOakLabDeliverParcelTextOffset,
             &result.deliver_parcel},
            {kOakLabRivalGrampsTextOffset,
             &result.rival_gramps},
            {kOakLabRivalWhatDidYouCallTextOffset,
             &result.rival_what_did_you_call},
            {kOakLabRequestTextOffset, &result.oak_request},
            {kOakLabPokedexInventionTextOffset,
             &result.pokedex_invention},
            {kOakLabGotPokedexTextOffset,
             &result.got_pokedex},
            {kOakLabDreamTextOffset, &result.oak_dream},
            {kOakLabRivalLeaveTextOffset,
             &result.rival_leave},
        }};
    for (const auto& [offset, text] : text_programs)
        if (!decode_text_program(rom, 0x07U, offset, *text) ||
            text->pages.empty()) {
            error =
                "Oak request dialogue could not be decoded from the pinned ROM";
            return false;
        }
    return true;
}

bool decode_route_22_first_rival(
    std::span<const std::uint8_t> rom,
    const std::array<StarterChoice, 3>& starters,
    const std::vector<ToggleActor>& toggle_actors,
    const OakReturnProgram& oak_return,
    Route22FirstRivalProgram& result, std::string& error) {
    result = {};

    if (!decode_checked_event(
            rom, kRoute22WantsBattleCheckOffset,
            result.wants_battle_flag, error) ||
        kRoute22BattleCoordsOffset + 5U > rom.size() ||
        rom[kRoute22BattleCoordsOffset + 4U] != 0xFFU) {
        if (error.empty())
            error =
                "Route 22 rival trigger does not match the verified ROM";
        return false;
    }
    const std::uint8_t upper_y =
        rom[kRoute22BattleCoordsOffset];
    const std::uint8_t upper_x =
        rom[kRoute22BattleCoordsOffset + 1U];
    const std::uint8_t lower_y =
        rom[kRoute22BattleCoordsOffset + 2U];
    const std::uint8_t lower_x =
        rom[kRoute22BattleCoordsOffset + 3U];
    if (upper_x != lower_x || lower_y != upper_y + 1U) {
        error =
            "Route 22 rival trigger is not the expected vertical rectangle";
        return false;
    }
    result.map_id = oak_return.route_22_rival.map_id;
    result.actor_index =
        oak_return.route_22_rival.actor_index;
    result.trigger_x = upper_x;
    result.trigger_y = upper_y;
    result.trigger_width = 1U;
    result.trigger_height =
        static_cast<std::uint8_t>(lower_y - upper_y + 1U);

    if (!decode_direct_npc_path(
            rom, kRoute22ApproachPathOffset,
            result.approach_upper, error) ||
        result.approach_upper.size() < 2U ||
        !std::ranges::all_of(
            result.approach_upper,
            [](PathCommand command) {
                return command == PathCommand::right;
            })) {
        if (error.empty())
            error =
                "Route 22 rival approach path is invalid";
        return false;
    }
    result.approach_lower = result.approach_upper;
    result.approach_lower.erase(
        result.approach_lower.begin());
    if (!decode_direct_npc_path(
            rom, kRoute22ExitPathUpperOffset,
            result.exit_upper, error) ||
        !decode_direct_npc_path(
            rom, kRoute22ExitPathLowerOffset,
            result.exit_lower, error))
        return false;

    constexpr std::array<std::uint8_t, 9> selector_tail{
        0xEAU, 0x59U, 0xD0U, 0x21U, 0xAFU,
        0x4FU, 0xCDU, 0xD6U, 0x4EU};
    if (kRoute22TrainerSelectorOffset + 11U > rom.size() ||
        rom[kRoute22TrainerSelectorOffset] != 0x3EU ||
        !has_bytes(
            rom, kRoute22TrainerSelectorOffset + 2U,
            selector_tail)) {
        error =
            "Route 22 rival trainer selector does not match the verified ROM";
        return false;
    }
    const std::uint8_t opponent =
        rom[kRoute22TrainerSelectorOffset + 1U];
    if (opponent <= kTrainerOpponentOffset) {
        error =
            "Route 22 rival trainer selector has an invalid class";
        return false;
    }
    const std::uint8_t trainer_class =
        static_cast<std::uint8_t>(
            opponent - kTrainerOpponentOffset);
    std::array<bool, 3> matched{};
    for (std::size_t record = 0U; record < 3U; ++record) {
        const std::size_t offset =
            kRoute22TrainerTableOffset + record * 2U;
        if (offset + 2U > rom.size() ||
            rom[offset + 1U] == 0U) {
            error =
                "Route 22 rival trainer table is truncated";
            return false;
        }
        const std::uint8_t species =
            dex_for_internal_species(rom, rom[offset]);
        const auto starter = std::ranges::find_if(
            starters,
            [species](const StarterChoice& choice) {
                return choice.rival_species == species;
            });
        if (species == 0U || starter == starters.end()) {
            error =
                "Route 22 rival trainer table references an unknown starter branch";
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(
            std::distance(starters.begin(), starter));
        if (matched[index]) {
            error =
                "Route 22 rival trainer table repeats a starter branch";
            return false;
        }
        matched[index] = true;
        result.battles[index] = {
            .rival_species = species,
            .trainer_class = trainer_class,
            .trainer_party = static_cast<std::uint16_t>(
                rom[offset + 1U] - 1U),
        };
    }
    if (!std::ranges::all_of(
            matched, [](bool value) { return value; })) {
        error =
            "Route 22 rival trainer table misses a starter branch";
        return false;
    }

    if (!decode_set_event(
            rom, kRoute22BeatRivalFlagOffset,
            result.beat_rival_flag, error) ||
        kRoute22ClearFlagsOffset + 7U > rom.size() ||
        rom[kRoute22ClearFlagsOffset] != 0x21U) {
        if (error.empty())
            error =
                "Route 22 rival event mutations do not match the verified ROM";
        return false;
    }
    const std::uint16_t flag_address =
        static_cast<std::uint16_t>(
            rom[kRoute22ClearFlagsOffset + 1U] |
            static_cast<std::uint16_t>(
                rom[kRoute22ClearFlagsOffset + 2U])
                << 8U);
    if (!decode_reused_event(
            rom, kRoute22ClearFlagsOffset + 3U,
            flag_address, false, result.first_rival_flag,
            error)) {
        return false;
    }
    std::uint32_t cleared_wants = 0U;
    if (!decode_reused_event(
            rom, kRoute22ClearFlagsOffset + 5U,
            flag_address, false, cleared_wants, error) ||
        result.first_rival_flag !=
            oak_return.first_route_22_rival_flag ||
        result.wants_battle_flag !=
            oak_return.route_22_rival_wants_battle_flag ||
        cleared_wants != result.wants_battle_flag) {
        if (error.empty())
            error =
                "Route 22 rival event paths disagree";
        return false;
    }

    ToggleActor hidden_rival;
    if (!decode_toggle_operation(
            rom, kRoute22HideRivalOffset, 0x11U,
            toggle_actors, hidden_rival, error) ||
        hidden_rival.map_id != result.map_id ||
        hidden_rival.actor_index != result.actor_index) {
        if (error.empty())
            error =
                "Route 22 rival hide operation disagrees with its imported actor";
        return false;
    }

    const std::array<std::pair<std::size_t, DecodedTextProgram*>, 4>
        text_programs{{
            {kRoute22BeforeBattleTextOffset,
             &result.before_battle},
            {kRoute22DefeatedTextOffset, &result.defeated},
            {kRoute22VictoryTextOffset, &result.victory},
            {kRoute22AfterBattleTextOffset,
             &result.after_battle},
        }};
    for (const auto& [offset, text] : text_programs)
        if (!decode_text_program(rom, 0x14U, offset, *text) ||
            text->pages.empty()) {
            error =
                "Route 22 rival dialogue could not be decoded from the pinned ROM";
            return false;
        }
    return true;
}

bool decode_oak_pokeball_program(
    std::span<const std::uint8_t> rom,
    const Route22FirstRivalProgram& route,
    OakPokeballProgram& result, std::string& error) {
    result = {};
    if (!decode_checked_event(
            rom, kOakLabRoute22BeatCheckOffset,
            result.beat_route_22_rival_flag, error)) {
        error = "Oak Pokeball gate: " + error;
        return false;
    }
    if (
        result.beat_route_22_rival_flag !=
            route.beat_rival_flag ||
        kOakLabPokeballGrantFlagOffset + 9U > rom.size() ||
        rom[kOakLabPokeballGrantFlagOffset] != 0x21U ||
        rom[kOakLabPokeballGrantFlagOffset + 3U] != 0xCBU ||
        rom[kOakLabPokeballGrantFlagOffset + 5U] != 0xCBU) {
        if (error.empty())
            error =
                "Oak Pokeball progression checks do not match the verified ROM";
        return false;
    }
    const std::uint16_t address =
        static_cast<std::uint16_t>(
            rom[kOakLabPokeballGrantFlagOffset + 1U] |
            static_cast<std::uint16_t>(
                rom[kOakLabPokeballGrantFlagOffset + 2U])
                << 8U);
    const std::uint8_t checked_opcode =
        rom[kOakLabPokeballGrantFlagOffset + 4U];
    const std::uint8_t set_opcode =
        rom[kOakLabPokeballGrantFlagOffset + 6U];
    if (checked_opcode < 0x46U ||
        (checked_opcode - 0x46U) % 8U != 0U ||
        set_opcode < 0xC6U ||
        (set_opcode - 0xC6U) % 8U != 0U ||
        (checked_opcode - 0x46U) / 8U !=
            (set_opcode - 0xC6U) / 8U) {
        error =
            "Oak Pokeball check-and-set event is invalid";
        return false;
    }
    result.got_pokeballs_flag =
        static_cast<std::uint32_t>(address) * 8U +
        (checked_opcode - 0x46U) / 8U;

    constexpr std::array<std::uint8_t, 3> give_item_call{
        0xCDU, 0x2EU, 0x3EU};
    if (kOakLabPokeballGrantOffset + 6U > rom.size() ||
        rom[kOakLabPokeballGrantOffset] != 0x01U ||
        !has_bytes(
            rom, kOakLabPokeballGrantOffset + 3U,
            give_item_call)) {
        error =
            "Oak Pokeball item grant does not match the verified ROM";
        return false;
    }
    result.quantity =
        rom[kOakLabPokeballGrantOffset + 1U];
    result.item_id =
        rom[kOakLabPokeballGrantOffset + 2U];
    if (result.quantity == 0U || result.item_id == 0U ||
        !decode_text_program(
            rom, 0x07U, kOakLabGivePokeballsTextOffset,
            result.give_pokeballs) ||
        !decode_text_program(
            rom, 0x07U, kOakLabComeSeeMeTextOffset,
            result.come_see_me) ||
        result.give_pokeballs.pages.empty() ||
        result.come_see_me.pages.empty()) {
        if (error.empty())
            error =
                "Oak Pokeball dialogue could not be decoded from the pinned ROM";
        return false;
    }
    return true;
}

bool decode_daisy_town_map_program(
    std::span<const std::uint8_t> rom,
    const std::vector<ToggleActor>& toggle_actors,
    const OakReturnProgram& oak_return,
    DaisyTownMapProgram& result, std::string& error) {
    result = {};
    if (!decode_checked_event(
            rom, kBluesHouseDaisyChecksOffset,
            result.got_town_map_flag, error)) {
        error = "Daisy Town Map owned check: " + error;
        return false;
    }
    if (!decode_checked_event(
            rom, kBluesHouseDaisyChecksOffset + 7U,
            result.got_pokedex_flag, error)) {
        error = "Daisy Pokedex check: " + error;
        return false;
    }
    if (
        result.got_pokedex_flag !=
            oak_return.got_pokedex_flag) {
        if (error.empty())
            error =
                "Daisy Town Map predicates do not match the verified ROM";
        return false;
    }

    constexpr std::array<std::uint8_t, 3> give_item_call{
        0xCDU, 0x2EU, 0x3EU};
    if (kBluesHouseTownMapGrantOffset + 6U > rom.size() ||
        rom[kBluesHouseTownMapGrantOffset] != 0x01U ||
        !has_bytes(
            rom, kBluesHouseTownMapGrantOffset + 3U,
            give_item_call)) {
        error =
            "Daisy Town Map item grant does not match the verified ROM";
        return false;
    }
    result.quantity =
        rom[kBluesHouseTownMapGrantOffset + 1U];
    result.item_id =
        rom[kBluesHouseTownMapGrantOffset + 2U];
    if (result.quantity == 0U || result.item_id == 0U ||
        !decode_item_name(
            rom, result.item_id, result.item_name, error))
        return false;

    std::uint32_t set_town_map_flag = 0U;
    if (!decode_set_event(
            rom, kBluesHouseTownMapFlagOffset,
            set_town_map_flag, error) ||
        set_town_map_flag != result.got_town_map_flag ||
        !decode_toggle_operation(
            rom, kBluesHouseTownMapToggleOffset, 0x11U,
            toggle_actors, result.town_map_actor, error))
        return false;

    const std::array<std::pair<std::size_t, DecodedTextProgram*>, 5>
        text_programs{{
            {kBluesHouseRivalAtLabTextOffset,
             &result.rival_at_lab},
            {kBluesHouseOfferMapTextOffset, &result.offer_map},
            {kBluesHouseGotMapTextOffset, &result.got_map},
            {kBluesHouseBagFullTextOffset, &result.bag_full},
            {kBluesHouseUseMapTextOffset, &result.use_map},
        }};
    for (const auto& [offset, text] : text_programs)
        if (!decode_text_program(rom, 0x06U, offset, *text) ||
            text->pages.empty()) {
            error =
                "Daisy Town Map dialogue could not be decoded from the pinned ROM";
            return false;
        }
    resolve_item_name_buffer(
        result.got_map, result.item_name);
    return true;
}

bool decode_pallet_reward_updates(
    std::span<const std::uint8_t> rom,
    const std::vector<ToggleActor>& toggle_actors,
    const OakPokeballProgram& oak,
    const DaisyTownMapProgram& daisy,
    PalletRewardUpdates& result, std::string& error) {
    result = {};
    if (!decode_set_event(
            rom, kBluesHouseEnteredFlagOffset,
            result.entered_blues_house_flag, error) ||
        !decode_checked_event(
            rom, kPalletGotPokeballsCheckOffset,
            result.got_pokeballs_flag, error) ||
        !decode_set_event(
            rom, kPalletAfterPokeballsFlagOffset,
            result.pallet_after_pokeballs_flag, error) ||
        !decode_checked_event(
            rom, kPalletDaisyWalkingCheckOffset,
            result.daisy_walking_flag, error) ||
        !decode_set_event(
            rom, kPalletDaisyWalkingFlagOffset,
            result.daisy_walking_flag, error) ||
        !decode_set_event(
            rom, kPalletAfterPokeballs2FlagOffset,
            result.pallet_after_pokeballs_2_flag,
            error)) {
        if (error.empty())
            error =
                "Pallet reward updates do not match the verified ROM";
        return false;
    }
    constexpr std::array<std::uint8_t, 4> both_events{
        0xE6U, 0x03U, 0xFEU, 0x03U};
    if (!has_bytes(
            rom, kPalletDaisyWalkingCheckOffset + 7U,
            both_events) ||
        result.got_pokeballs_flag !=
            oak.got_pokeballs_flag) {
        error =
            "Pallet reward predicates disagree with imported rewards";
        return false;
    }
    result.got_town_map_flag =
        daisy.got_town_map_flag;
    if (!decode_toggle_operation(
            rom, kPalletDaisySittingToggleOffset, 0x11U,
            toggle_actors, result.daisy_sitting, error) ||
        !decode_toggle_operation(
            rom, kPalletDaisyWalkingToggleOffset, 0x15U,
            toggle_actors, result.daisy_walking, error, true))
        return false;
    if (result.daisy_sitting.map_id !=
            result.daisy_walking.map_id ||
        result.daisy_sitting.map_id !=
            daisy.town_map_actor.map_id) {
        error =
            "Pallet Daisy transition has inconsistent actor owners";
        return false;
    }
    return true;
}

bool decode_route_1_potion_program(
    std::span<const std::uint8_t> rom,
    Route1PotionProgram& result, std::string& error) {
    result = {};
    if (kRoute1PotionProgramOffset + 7U > rom.size() ||
        rom[kRoute1PotionProgramOffset] != 0x21U ||
        rom[kRoute1PotionProgramOffset + 3U] != 0xCBU ||
        rom[kRoute1PotionProgramOffset + 4U] != 0x46U ||
        rom[kRoute1PotionProgramOffset + 5U] != 0xCBU ||
        rom[kRoute1PotionProgramOffset + 6U] != 0xC6U) {
        error =
            "Route 1 potion event does not match the verified ROM";
        return false;
    }
    const std::uint16_t address =
        static_cast<std::uint16_t>(
            rom[kRoute1PotionProgramOffset + 1U] |
            static_cast<std::uint16_t>(
                rom[kRoute1PotionProgramOffset + 2U])
                << 8U);
    result.got_potion_sample_flag =
        static_cast<std::uint32_t>(address) * 8U;

    constexpr std::array<std::uint8_t, 3> give_item_call{
        0xCDU, 0x2EU, 0x3EU};
    if (kRoute1PotionGrantOffset + 6U > rom.size() ||
        rom[kRoute1PotionGrantOffset] != 0x01U ||
        !has_bytes(
            rom, kRoute1PotionGrantOffset + 3U,
            give_item_call)) {
        error =
            "Route 1 potion item grant does not match the verified ROM";
        return false;
    }
    result.quantity = rom[kRoute1PotionGrantOffset + 1U];
    result.item_id = rom[kRoute1PotionGrantOffset + 2U];
    if (result.quantity == 0U || result.item_id == 0U ||
        !decode_item_name(
            rom, result.item_id, result.item_name, error))
        return false;

    const std::array<std::pair<std::size_t, DecodedTextProgram*>, 4>
        text_programs{{
            {kRoute1MartSampleTextOffset, &result.sample},
            {kRoute1GotPotionTextOffset, &result.got_item},
            {kRoute1RepeatTextOffset, &result.repeat},
            {kRoute1NoRoomTextOffset, &result.no_room},
        }};
    for (const auto& [offset, text] : text_programs)
        if (!decode_text_program(rom, 0x07U, offset, *text) ||
            text->pages.empty()) {
            error =
                "Route 1 potion dialogue could not be decoded from the pinned ROM";
            return false;
        }
    resolve_item_name_buffer(
        result.got_item, result.item_name);
    return true;
}

bool decode_pewter_gym_program(
    std::span<const std::uint8_t> rom,
    const std::vector<ToggleActor>& toggle_actors,
    const std::vector<ImportedItemName>& item_names,
    const Route22FirstRivalProgram& route_22,
    PewterGymProgram& result, std::string& error) {
    result = {};
    result.map_id = 54U;
    result.brock_actor_index = 1U;
    result.guide_actor_index = 3U;

    constexpr std::array<std::uint8_t, 5> post_battle_check{
        0xFAU, 0x57U, 0xD0U, 0xFEU, 0xFFU};
    if (!has_bytes(
            rom, kPewterGymBrockPostBattleOffset,
            post_battle_check) ||
        kPewterGymBrockActorOffset + 8U > rom.size() ||
        rom[kPewterGymBrockActorOffset + 3U] != 0xFFU ||
        (rom[kPewterGymBrockActorOffset + 5U] & 0xC0U) !=
            0x40U ||
        (rom[kPewterGymBrockActorOffset + 5U] & 0x3FU) !=
            result.brock_actor_index ||
        rom[kPewterGymBrockActorOffset + 6U] <=
            kTrainerOpponentOffset ||
        rom[kPewterGymBrockActorOffset + 7U] == 0U) {
        error =
            "Pewter Gym Brock actor does not match the verified ROM";
        return false;
    }
    result.trainer_class = static_cast<std::uint8_t>(
        rom[kPewterGymBrockActorOffset + 6U] -
        kTrainerOpponentOffset);
    result.trainer_party = static_cast<std::uint16_t>(
        rom[kPewterGymBrockActorOffset + 7U] - 1U);

    if (!decode_checked_event(
            rom, kPewterGymBrockCheckOffset,
            result.beat_brock_flag, error) ||
        kPewterGymGotTmCheckOffset + 2U > rom.size() ||
        rom[kPewterGymGotTmCheckOffset] != 0xCBU ||
        rom[kPewterGymGotTmCheckOffset + 1U] < 0x47U ||
        (rom[kPewterGymGotTmCheckOffset + 1U] - 0x47U) % 8U !=
            0U) {
        if (error.empty())
            error =
                "Pewter Gym Brock reward checks do not match the verified ROM";
        return false;
    }
    result.got_tm_flag =
        result.beat_brock_flag -
        (result.beat_brock_flag % 8U) +
        (rom[kPewterGymGotTmCheckOffset + 1U] - 0x47U) / 8U;

    std::uint32_t set_beat_brock = 0U;
    std::uint32_t set_got_tm = 0U;
    if (!decode_set_event(
            rom, kPewterGymBeatBrockSetOffset,
            set_beat_brock, error) ||
        !decode_set_event(
            rom, kPewterGymGotTmSetOffset,
            set_got_tm, error) ||
        set_beat_brock != result.beat_brock_flag ||
        set_got_tm != result.got_tm_flag ||
        !decode_set_event(
            rom, kPewterGymObtainedBadgeSetOffset,
            result.obtained_badge_flag, error) ||
        !decode_set_event(
            rom, kPewterGymBeatGymSetOffset,
            result.beat_gym_flag, error) ||
        !decode_set_event(
            rom, kPewterGymTrainerDefeatedSetOffset,
            result.gym_trainer_defeated_flag, error)) {
        if (error.empty())
            error =
                "Pewter Gym reward mutations do not match the verified ROM";
        return false;
    }

    constexpr std::array<std::uint8_t, 3> give_item_call{
        0xCDU, 0x2EU, 0x3EU};
    constexpr std::array<std::uint8_t, 7> bag_full_text{
        0x3EU, 0x06U, 0xE0U, 0x8CU, 0xCDU, 0x20U, 0x29U};
    if (kPewterGymTmGrantOffset + 6U > rom.size() ||
        rom[kPewterGymTmGrantOffset] != 0x01U ||
        !has_bytes(
            rom, kPewterGymTmGrantOffset + 3U,
            give_item_call) ||
        !has_bytes(
            rom, kPewterGymBagFullOffset,
            bag_full_text)) {
        error =
            "Pewter Gym TM grant does not match the verified ROM";
        return false;
    }
    result.tm_quantity = rom[kPewterGymTmGrantOffset + 1U];
    result.tm_item_id = rom[kPewterGymTmGrantOffset + 2U];
    const auto item = std::ranges::find_if(
        item_names,
        [&result](const ImportedItemName& candidate) {
            return candidate.item_id == result.tm_item_id;
        });
    if (result.tm_quantity == 0U || item == item_names.end()) {
        error =
            "Pewter Gym TM is absent from the imported item catalogue";
        return false;
    }
    result.tm_name = item->name;

    if (!decode_toggle_operation(
            rom, kPewterGymHideGymGuideOffset, 0x11U,
            toggle_actors, result.gym_guide, error) ||
        !decode_toggle_operation(
            rom, kPewterGymHideRouteRivalOffset, 0x11U,
            toggle_actors, result.route_rival, error))
        return false;

    if (kPewterGymClearRouteFlagsOffset + 7U > rom.size() ||
        rom[kPewterGymClearRouteFlagsOffset] != 0x21U) {
        error =
            "Pewter Gym Route 22 reset does not match the verified ROM";
        return false;
    }
    const std::uint16_t route_flag_address =
        static_cast<std::uint16_t>(
            rom[kPewterGymClearRouteFlagsOffset + 1U] |
            static_cast<std::uint16_t>(
                rom[kPewterGymClearRouteFlagsOffset + 2U])
                << 8U);
    if (!decode_reused_event(
            rom, kPewterGymClearRouteFlagsOffset + 3U,
            route_flag_address, false,
            result.first_route_rival_flag, error) ||
        !decode_reused_event(
            rom, kPewterGymClearRouteFlagsOffset + 5U,
            route_flag_address, false,
            result.route_rival_wants_battle_flag, error) ||
        result.first_route_rival_flag !=
            route_22.first_rival_flag ||
        result.route_rival_wants_battle_flag !=
            route_22.wants_battle_flag ||
        result.route_rival.map_id != route_22.map_id ||
        result.route_rival.actor_index !=
            route_22.actor_index) {
        if (error.empty())
            error =
                "Pewter Gym Route 22 reset disagrees with imported Route 22 state";
        return false;
    }

    const std::array<std::pair<std::size_t, DecodedTextProgram*>, 11>
        text_programs{{
            {kPewterGymBrockPreBattleTextOffset,
             &result.pre_battle},
            {kPewterGymBadgeTextOffset, &result.badge},
            {kPewterGymWaitTakeThisTextOffset,
             &result.wait_take_this},
            {kPewterGymReceivedTmTextOffset,
             &result.received_tm},
            {kPewterGymTmNoRoomTextOffset, &result.no_room},
            {kPewterGymBrockAdviceTextOffset,
             &result.post_battle_advice},
            {kPewterGymGuidePreAdviceTextOffset,
             &result.guide_pre_advice},
            {kPewterGymGuideBeginAdviceTextOffset,
             &result.guide_begin_advice},
            {kPewterGymGuideAdviceTextOffset,
             &result.guide_advice},
            {kPewterGymGuideFreeServiceTextOffset,
             &result.guide_free_service},
            {kPewterGymGuidePostBattleTextOffset,
             &result.guide_post_battle},
        }};
    for (const auto& [offset, text] : text_programs)
        if (!decode_text_program(rom, 0x17U, offset, *text) ||
            !text->complete || text->pages.empty()) {
            error =
                "Pewter Gym dialogue could not be decoded from the pinned ROM";
            return false;
        }
    return true;
}

bool decode_pewter_city_program(
    std::span<const std::uint8_t> rom,
    const PewterGymProgram& gym, PewterCityProgram& result,
    std::string& error) {
    result = {};
    result.map_id = 2U;
    result.museum_actor_index = 3U;
    result.repel_actor_index = 4U;
    result.gym_guide_actor_index = 5U;
    result.beat_brock_flag = gym.beat_brock_flag;
    if (gym.gym_guide.map_id != result.map_id ||
        gym.gym_guide.actor_index !=
            result.gym_guide_actor_index ||
        kPewterCityObjectOffset + 2U > rom.size()) {
        error =
            "Pewter City guide state disagrees with imported Pewter Gym state";
        return false;
    }

    std::size_t cursor = kPewterCityObjectOffset + 1U;
    const std::uint8_t warp_count = rom[cursor++];
    if (warp_count != 7U ||
        cursor + static_cast<std::size_t>(warp_count) * 4U >
            rom.size()) {
        error =
            "Pewter City warp table does not match the verified ROM";
        return false;
    }
    for (std::uint8_t index = 1U; index <= warp_count; ++index) {
        const std::uint8_t y = rom[cursor];
        const std::uint8_t x = rom[cursor + 1U];
        if (index == 1U)
            result.museum_player_target = {x,
                static_cast<std::uint8_t>(y + 1U)};
        else if (index == 3U)
            result.gym_player_target = {x,
                static_cast<std::uint8_t>(y + 1U)};
        cursor += 4U;
    }
    if (cursor >= rom.size()) {
        error =
            "Pewter City object table is truncated after its warps";
        return false;
    }
    const std::uint8_t background_count = rom[cursor++];
    if (background_count != 7U ||
        cursor + static_cast<std::size_t>(background_count) * 3U + 1U >
            rom.size()) {
        error =
            "Pewter City background table does not match the verified ROM";
        return false;
    }
    cursor += static_cast<std::size_t>(background_count) * 3U;
    const std::uint8_t actor_count = rom[cursor++];
    if (actor_count != 5U) {
        error =
            "Pewter City actor count does not match the verified ROM";
        return false;
    }
    for (std::uint8_t actor_index = 1U;
         actor_index <= actor_count; ++actor_index) {
        if (cursor + 6U > rom.size()) {
            error = "Pewter City actor table is truncated";
            return false;
        }
        ++cursor; // sprite id
        const std::uint8_t stored_y = rom[cursor++];
        const std::uint8_t stored_x = rom[cursor++];
        cursor += 2U; // movement and facing/axis
        const std::uint8_t encoded_text = rom[cursor++];
        if (stored_x < 4U || stored_y < 4U) {
            error = "Pewter City actor has an invalid biased position";
            return false;
        }
        const PewterCityProgram::Position position{
            static_cast<std::uint8_t>(stored_x - 4U),
            static_cast<std::uint8_t>(stored_y - 4U)};
        if (actor_index == result.museum_actor_index)
            result.museum_actor_origin = position;
        else if (actor_index == result.repel_actor_index)
            result.repel_actor_origin = position;
        else if (actor_index == result.gym_guide_actor_index)
            result.gym_guide_actor_origin = position;
        const std::uint8_t kind =
            static_cast<std::uint8_t>(encoded_text & 0xC0U);
        const std::size_t parameters =
            kind == 0x40U ? 2U : kind == 0x80U ? 1U : 0U;
        if (kind == 0xC0U || cursor + parameters > rom.size()) {
            error =
                "Pewter City actor parameters do not match the verified ROM";
            return false;
        }
        cursor += parameters;
    }

    cursor = kPewterCityEastGateCoordsOffset;
    for (PewterCityProgram::Position& cell :
         result.east_gate_cells) {
        if (cursor + 2U > rom.size() ||
            rom[cursor] == 0xFFU) {
            error =
                "Pewter City east-gate coordinate table is truncated";
            return false;
        }
        cell = {rom[cursor + 1U], rom[cursor]};
        cursor += 2U;
    }
    if (cursor >= rom.size() || rom[cursor] != 0xFFU) {
        error =
            "Pewter City east-gate coordinate table has an unexpected extent";
        return false;
    }

    const std::array<std::pair<std::size_t, DecodedTextProgram*>, 9>
        text_programs{{
            {kPewterCityMuseumQuestionTextOffset,
             &result.museum_question},
            {kPewterCityMuseumYesTextOffset, &result.museum_yes},
            {kPewterCityMuseumNoTextOffset, &result.museum_no},
            {kPewterCityMuseumArrivalTextOffset,
             &result.museum_arrival},
            {kPewterCityRepelQuestionTextOffset,
             &result.repel_question},
            {kPewterCityRepelYesTextOffset, &result.repel_yes},
            {kPewterCityRepelNoTextOffset, &result.repel_no},
            {kPewterCityGymFollowTextOffset, &result.gym_follow},
            {kPewterCityGymArrivalTextOffset,
             &result.gym_arrival},
        }};
    for (const auto& [offset, text] : text_programs)
        if (!decode_text_program(rom, 0x06U, offset, *text) ||
            !text->complete || text->pages.empty()) {
            error =
                "Pewter City dialogue could not be decoded from the pinned ROM";
            return false;
        }
    return true;
}

bool decode_mt_moon_fossil_program(
    std::span<const std::uint8_t> rom,
    const std::vector<ToggleActor>& toggle_actors,
    const std::vector<ImportedItemName>& item_names,
    MtMoonFossilProgram& result, std::string& error) {
    result = {};
    result.map_id = 61U;
    result.super_nerd_actor_index = 1U;
    result.dome_actor_index = 6U;
    result.helix_actor_index = 7U;
    if (kMtMoonB2FDefaultScriptOffset + 21U > rom.size() ||
        kMtMoonB2FSuperNerdActorOffset + 8U > rom.size() ||
        rom[kMtMoonB2FDefaultScriptOffset + 8U] != 0xFAU ||
        rom[kMtMoonB2FDefaultScriptOffset + 11U] != 0xFEU ||
        rom[kMtMoonB2FDefaultScriptOffset + 16U] != 0xFAU ||
        rom[kMtMoonB2FDefaultScriptOffset + 19U] != 0xFEU ||
        (rom[kMtMoonB2FSuperNerdActorOffset + 5U] & 0xC0U) !=
            0x40U ||
        (rom[kMtMoonB2FSuperNerdActorOffset + 5U] & 0x3FU) !=
            result.super_nerd_actor_index ||
        rom[kMtMoonB2FSuperNerdActorOffset + 6U] <=
            kTrainerOpponentOffset ||
        rom[kMtMoonB2FSuperNerdActorOffset + 7U] == 0U) {
        error =
            "Mt. Moon fossil gate does not match the verified ROM";
        return false;
    }
    result.trigger_y =
        rom[kMtMoonB2FDefaultScriptOffset + 12U];
    result.trigger_x =
        rom[kMtMoonB2FDefaultScriptOffset + 20U];
    result.trainer_class = static_cast<std::uint8_t>(
        rom[kMtMoonB2FSuperNerdActorOffset + 6U] -
        kTrainerOpponentOffset);
    result.trainer_party = static_cast<std::uint16_t>(
        rom[kMtMoonB2FSuperNerdActorOffset + 7U] - 1U);
    std::size_t suppression_cursor =
        kMtMoonB2FFossilAreaCoordsOffset;
    std::vector<std::pair<std::uint8_t, std::uint8_t>>
        suppression_cells;
    while (suppression_cursor < rom.size() &&
           rom[suppression_cursor] != 0xFFU &&
           suppression_cells.size() < 64U) {
        if (suppression_cursor + 2U > rom.size()) break;
        suppression_cells.emplace_back(
            rom[suppression_cursor + 1U],
            rom[suppression_cursor]);
        suppression_cursor += 2U;
    }
    if (suppression_cursor >= rom.size() ||
        rom[suppression_cursor] != 0xFFU ||
        suppression_cells.empty()) {
        error =
            "Mt. Moon fossil encounter-suppression cells have an unexpected extent";
        return false;
    }
    const auto [minimum_x, maximum_x] = std::ranges::minmax(
        suppression_cells, {}, &std::pair<std::uint8_t, std::uint8_t>::first);
    const auto [minimum_y, maximum_y] = std::ranges::minmax(
        suppression_cells, {}, &std::pair<std::uint8_t, std::uint8_t>::second);
    result.suppression_x = minimum_x.first;
    result.suppression_y = minimum_y.second;
    result.suppression_width = static_cast<std::uint8_t>(
        maximum_x.first - minimum_x.first + 1U);
    result.suppression_height = static_cast<std::uint8_t>(
        maximum_y.second - minimum_y.second + 1U);
    if (suppression_cells.size() !=
        static_cast<std::size_t>(result.suppression_width) *
            result.suppression_height) {
        error =
            "Mt. Moon fossil encounter-suppression cells do not form a rectangle";
        return false;
    }
    for (std::size_t index = 0U;
         index < suppression_cells.size(); ++index) {
        const std::uint8_t expected_x =
            static_cast<std::uint8_t>(
                result.suppression_x +
                index % result.suppression_width);
        const std::uint8_t expected_y =
            static_cast<std::uint8_t>(
                result.suppression_y +
                index / result.suppression_width);
        if (suppression_cells[index] !=
            std::pair{expected_x, expected_y}) {
            error =
                "Mt. Moon fossil encounter-suppression cells are not row-major";
            return false;
        }
    }

    std::uint32_t set_beat_super_nerd = 0U;
    if (!decode_checked_event(
            rom, kMtMoonB2FDefaultScriptOffset,
            result.beat_super_nerd_flag, error) ||
        !decode_set_event(
            rom, kMtMoonB2FBeatSuperNerdSetOffset,
            set_beat_super_nerd, error) ||
        set_beat_super_nerd != result.beat_super_nerd_flag ||
        !decode_set_event(
            rom, kMtMoonB2FDomeSetOffset,
            result.got_dome_flag, error) ||
        !decode_set_event(
            rom, kMtMoonB2FHelixSetOffset,
            result.got_helix_flag, error))
        return false;

    constexpr std::array<std::uint8_t, 3> give_item_call{
        0xCDU, 0x2EU, 0x3EU};
    const auto decode_grant =
        [&](std::size_t offset, std::uint8_t& quantity,
            std::uint16_t& item_id, std::string& item_name) {
            if (offset + 6U > rom.size() ||
                rom[offset] != 0x01U ||
                !has_bytes(rom, offset + 3U, give_item_call))
                return false;
            quantity = rom[offset + 1U];
            item_id = rom[offset + 2U];
            const auto item = std::ranges::find_if(
                item_names,
                [item_id](const ImportedItemName& candidate) {
                    return candidate.item_id == item_id;
                });
            if (quantity == 0U || item == item_names.end())
                return false;
            item_name = item->name;
            return true;
        };
    if (!decode_grant(
            kMtMoonB2FDomeGrantOffset, result.dome_quantity,
            result.dome_item_id, result.dome_item_name) ||
        !decode_grant(
            kMtMoonB2FHelixGrantOffset, result.helix_quantity,
            result.helix_item_id, result.helix_item_name) ||
        !decode_toggle_operation(
            rom, kMtMoonB2FDomeToggleOffset, 0x11U,
            toggle_actors, result.dome_actor, error) ||
        !decode_toggle_operation(
            rom, kMtMoonB2FHelixToggleOffset, 0x11U,
            toggle_actors, result.helix_actor, error) ||
        result.dome_actor.map_id != result.map_id ||
        result.dome_actor.actor_index != result.dome_actor_index ||
        result.helix_actor.map_id != result.map_id ||
        result.helix_actor.actor_index != result.helix_actor_index) {
        if (error.empty())
            error =
                "Mt. Moon fossil grants or actor toggles do not match the verified ROM";
        return false;
    }

    DecodedTextProgram received;
    const std::array<std::pair<std::size_t, DecodedTextProgram*>, 8>
        text_programs{{
            {kMtMoonB2FBothMineTextOffset, &result.both_mine},
            {kMtMoonB2FOkShareTextOffset, &result.okay_share},
            {kMtMoonB2FEachTakeOneTextOffset,
             &result.each_take_one},
            {kMtMoonB2FPokemonLabTextOffset, &result.pokemon_lab},
            {kMtMoonB2FDomeQuestionTextOffset,
             &result.dome_question},
            {kMtMoonB2FHelixQuestionTextOffset,
             &result.helix_question},
            {kMtMoonB2FNoRoomTextOffset, &result.no_room},
            {kMtMoonB2FThenMineTextOffset, &result.then_mine},
        }};
    for (const auto& [offset, text] : text_programs)
        if (!decode_text_program(rom, 0x12U, offset, *text) ||
            !text->complete || text->pages.empty()) {
            error =
                "Mt. Moon fossil dialogue could not be decoded from the pinned ROM";
            return false;
        }
    if (!decode_text_program(
            rom, 0x12U, kMtMoonB2FReceivedTextOffset,
            received) ||
        !received.complete || received.pages.empty()) {
        error =
            "Mt. Moon fossil receipt text could not be decoded from the pinned ROM";
        return false;
    }
    result.dome_received = received;
    result.helix_received = std::move(received);
    resolve_item_name_buffer(
        result.dome_received, result.dome_item_name);
    resolve_item_name_buffer(
        result.helix_received, result.helix_item_name);
    return true;
}

bool decode_mt_moon_magikarp_program(
    std::span<const std::uint8_t> rom,
    MtMoonMagikarpProgram& result, std::string& error) {
    result = {};
    constexpr std::uint8_t map_id = 68U;
    constexpr std::uint8_t salesman_text_id = 4U;
    constexpr std::array<std::uint8_t, 8> price_signature{
        0xE0U, 0x9FU, 0xE0U, 0xA1U,
        0x3EU, 0x05U, 0xE0U, 0xA0U};
    constexpr std::array<std::uint8_t, 7> event_check_signature{
        0xFAU, 0xC6U, 0xD7U, 0x87U,
        0xDAU, 0x53U, 0x53U};
    if (!has_bytes(
            rom, kMtMoonMagikarpEventCheckOffset,
            event_check_signature) ||
        !has_bytes(
            rom, kMtMoonMagikarpPriceOffset,
            price_signature) ||
        kMtMoonMagikarpGrantOffset + 3U > rom.size() ||
        rom[kMtMoonMagikarpGrantOffset] != 0x01U ||
        kMtMoonPokecenterObjectOffset + 12U > rom.size() ||
        rom[kMtMoonPokecenterObjectOffset + 1U] != 2U ||
        rom[kMtMoonPokecenterObjectOffset + 10U] != 0U ||
        rom[kMtMoonPokecenterObjectOffset + 11U] == 0U) {
        error =
            "Mt. Moon Magikarp sale control flow does not match the verified ROM";
        return false;
    }

    // Resolve the script owner from the object records. The imported
    // campaign program never relies on an engine-side Red actor constant.
    const std::uint8_t object_count =
        rom[kMtMoonPokecenterObjectOffset + 11U];
    std::size_t object_cursor =
        kMtMoonPokecenterObjectOffset + 12U;
    for (std::uint8_t actor = 1U;
         actor <= object_count; ++actor) {
        if (object_cursor + 6U > rom.size()) {
            error =
                "Mt. Moon Pokecenter object table is truncated";
            return false;
        }
        const std::uint8_t text_flags =
            rom[object_cursor + 5U];
        if ((text_flags & 0x3FU) == salesman_text_id) {
            if (result.actor_index != 0U) {
                error =
                    "Mt. Moon Magikarp salesman owner is ambiguous";
                return false;
            }
            result.actor_index = actor;
        }
        object_cursor +=
            (text_flags & 0xC0U) == 0U ? 6U : 8U;
    }
    if (result.actor_index == 0U) {
        error =
            "Mt. Moon Magikarp salesman owner is missing";
        return false;
    }

    const std::uint16_t event_address =
        static_cast<std::uint16_t>(
            rom[kMtMoonMagikarpEventCheckOffset + 1U] |
            static_cast<std::uint16_t>(
                rom[kMtMoonMagikarpEventCheckOffset + 2U])
                << 8U);
    result.map_id = map_id;
    result.bought_flag =
        static_cast<std::uint32_t>(event_address) * 8U + 7U;
    std::uint32_t set_flag = 0U;
    if (!decode_set_event(
            rom, kMtMoonMagikarpEventSetOffset,
            set_flag, error) ||
        set_flag != result.bought_flag) {
        if (error.empty())
            error =
                "Mt. Moon Magikarp sale event branches disagree";
        return false;
    }

    const std::uint8_t price_bcd =
        rom[kMtMoonMagikarpPriceOffset + 5U];
    if ((price_bcd & 0x0FU) > 9U ||
        (price_bcd >> 4U) > 9U) {
        error =
            "Mt. Moon Magikarp price is not valid packed BCD";
        return false;
    }
    result.price = static_cast<std::uint32_t>(
                       (price_bcd >> 4U) * 10U +
                       (price_bcd & 0x0FU)) *
                   100U;
    result.level = rom[kMtMoonMagikarpGrantOffset + 1U];
    result.species_dex = dex_for_internal_species(
        rom, rom[kMtMoonMagikarpGrantOffset + 2U]);
    if (result.price == 0U || result.level == 0U ||
        result.species_dex == 0U) {
        error =
            "Mt. Moon Magikarp sale tuple is empty";
        return false;
    }

    const std::array<
        std::tuple<std::uint8_t, std::size_t,
                   DecodedTextProgram*>,
        7>
        text_programs{{
            {0x12U, kMtMoonMagikarpOfferTextOffset,
             &result.offer},
            {0x12U, kMtMoonMagikarpNoTextOffset,
             &result.chose_no},
            {0x12U, kMtMoonMagikarpNoMoneyTextOffset,
             &result.no_money},
            {0x12U, kMtMoonMagikarpNoRefundsTextOffset,
             &result.no_refunds},
            {0x13U, kGotPokemonTextOffset,
             &result.got_pokemon},
            {0x13U, kSentPokemonToBoxTextOffset,
             &result.sent_to_box},
            {0x13U, kPokemonBoxFullTextOffset,
             &result.box_full},
        }};
    for (const auto& [bank, offset, text] : text_programs)
        if (!decode_text_program(rom, bank, offset, *text) ||
            !text->complete || text->pages.empty()) {
            error =
                "Mt. Moon Magikarp sale dialogue could not be decoded from the pinned ROM";
            return false;
        }

    bool owns_got_name = false;
    for (std::string& page : result.got_pokemon.pages) {
        const std::size_t name = page.find("{name_buffer}");
        if (name == std::string::npos) continue;
        owns_got_name = true;
        // LINE terminates the preceding literal before the separate
        // text_ram command; preserve that imported cursor operation.
        if (name != 0U && page[name - 1U] != '\n')
            page.insert(name, "\n");
    }
    bool owns_box_nickname = false;
    bool owns_box_number = false;
    for (std::string& page : result.sent_to_box.pages) {
        const std::size_t nickname =
            page.find("{ram_de06}");
        owns_box_nickname =
            owns_box_nickname ||
            nickname != std::string::npos;
        owns_box_number =
            owns_box_number ||
            page.find("{ram_cf4b}") != std::string::npos;
        if (nickname != std::string::npos &&
            nickname != 0U &&
            page[nickname - 1U] != '\n')
            page.insert(nickname, "\n");
        replace_all(page, "{ram_de06}", "{name_buffer}");
        replace_all(page, "{ram_cf4b}", "{box_number}");
    }
    if (!owns_got_name || !owns_box_nickname ||
        !owns_box_number) {
        error =
            "Mt. Moon Magikarp result is missing its imported dynamic fields";
        return false;
    }
    return true;
}

bool decode_cerulean_rival_program(
    std::span<const std::uint8_t> rom,
    const std::array<StarterChoice, 3>& starters,
    const std::vector<ToggleActor>& toggle_actors,
    CeruleanRivalProgram& result, std::string& error) {
    result = {};
    constexpr std::uint8_t map_id = 3U;
    constexpr std::uint8_t rival_text_id = 1U;
    if (!decode_map_actor_owner(
            rom, kCeruleanCityObjectOffset,
            rival_text_id, result.actor_index, error) ||
        !decode_checked_event(
            rom, kCeruleanRivalEventCheckOffset,
            result.beat_flag, error) ||
        kCeruleanRivalTriggerCoordsOffset + 5U > rom.size() ||
        rom[kCeruleanRivalTriggerCoordsOffset + 4U] != 0xFFU) {
        if (error.empty())
            error =
                "Cerulean rival owner or trigger does not match the verified ROM";
        return false;
    }
    result.map_id = map_id;
    result.trigger_y =
        rom[kCeruleanRivalTriggerCoordsOffset];
    result.trigger_x =
        rom[kCeruleanRivalTriggerCoordsOffset + 1U];
    const std::uint8_t second_y =
        rom[kCeruleanRivalTriggerCoordsOffset + 2U];
    const std::uint8_t second_x =
        rom[kCeruleanRivalTriggerCoordsOffset + 3U];
    if (second_y != result.trigger_y ||
        second_x != result.trigger_x + 1U) {
        error =
            "Cerulean rival trigger is not the imported horizontal pair";
        return false;
    }
    result.trigger_width = 2U;

    if (!decode_direct_npc_path(
            rom, kCeruleanRivalApproachPathOffset,
            result.approach, error) ||
        result.approach.empty() ||
        !std::ranges::all_of(
            result.approach,
            [](PathCommand command) {
                return command == PathCommand::down;
            }) ||
        result.trigger_y <= result.approach.size() ||
        !decode_direct_npc_path(
            rom, kCeruleanRivalExitLeftOffset,
            result.exit_left, error) ||
        !decode_direct_npc_path(
            rom, kCeruleanRivalExitRightOffset,
            result.exit_right, error)) {
        if (error.empty())
            error =
                "Cerulean rival movement streams do not match the verified ROM";
        return false;
    }
    result.approach_start_y =
        static_cast<std::uint8_t>(
            result.trigger_y - result.approach.size() - 1U);

    std::uint32_t set_flag = 0U;
    if (!decode_set_event(
            rom, kCeruleanRivalEventSetOffset,
            set_flag, error) ||
        set_flag != result.beat_flag) {
        if (error.empty())
            error =
                "Cerulean rival event branches disagree";
        return false;
    }
    const auto toggle = std::ranges::find_if(
        toggle_actors,
        [&](const ToggleActor& actor) {
            return actor.map_id == result.map_id &&
                   actor.actor_index == result.actor_index;
        });
    if (toggle == toggle_actors.end() ||
        toggle->initially_visible) {
        error =
            "Cerulean rival is not initially hidden by the imported toggle table";
        return false;
    }

    constexpr std::array<std::pair<std::size_t, std::uint8_t>, 18>
        selector_signature{{
            {0U, 0x3EU},  {2U, 0xEAU},  {3U, 0x59U},
            {4U, 0xD0U},  {5U, 0xFAU},  {6U, 0x15U},
            {7U, 0xD7U},  {8U, 0xFEU},  {10U, 0x20U},
            {12U, 0x3EU}, {14U, 0x18U}, {16U, 0xFEU},
            {18U, 0x20U}, {20U, 0x3EU}, {22U, 0x18U},
            {24U, 0x3EU}, {26U, 0xEAU}, {27U, 0x5DU},
        }};
    if (kCeruleanRivalSelectorOffset + 29U > rom.size()) {
        error =
            "Cerulean rival trainer selector is truncated";
        return false;
    }
    for (const auto& [relative, expected] :
         selector_signature)
        if (rom[kCeruleanRivalSelectorOffset + relative] !=
            expected) {
            error =
                "Cerulean rival trainer selector does not match the verified ROM";
            return false;
        }
    const std::uint8_t opponent =
        rom[kCeruleanRivalSelectorOffset + 1U];
    if (opponent <= kTrainerOpponentOffset) {
        error =
            "Cerulean rival trainer class is invalid";
        return false;
    }
    const std::uint8_t trainer_class =
        static_cast<std::uint8_t>(
            opponent - kTrainerOpponentOffset);
    const std::uint8_t first_species =
        dex_for_internal_species(
            rom, rom[kCeruleanRivalSelectorOffset + 9U]);
    const std::uint8_t second_species =
        dex_for_internal_species(
            rom, rom[kCeruleanRivalSelectorOffset + 17U]);
    const std::array<std::uint8_t, 3> one_based_parties{
        rom[kCeruleanRivalSelectorOffset + 13U],
        rom[kCeruleanRivalSelectorOffset + 21U],
        rom[kCeruleanRivalSelectorOffset + 25U],
    };
    std::array<bool, 3> assigned{};
    for (std::size_t index = 0U;
         index < starters.size(); ++index) {
        const std::uint8_t species =
            starters[index].rival_species;
        const std::size_t party =
            species == first_species
                ? 0U
                : species == second_species ? 1U : 2U;
        if (one_based_parties[party] == 0U) {
            error =
                "Cerulean rival trainer party is invalid";
            return false;
        }
        result.battles[index] = {
            .rival_species = species,
            .trainer_class = trainer_class,
            .trainer_party =
                static_cast<std::uint16_t>(
                    one_based_parties[party] - 1U),
        };
        assigned[party] = true;
    }
    if (!std::ranges::all_of(
            assigned, [](bool value) { return value; })) {
        error =
            "Cerulean rival selector does not cover all starter branches";
        return false;
    }

    const std::array<
        std::pair<std::size_t, DecodedTextProgram*>, 4>
        text_programs{{
            {kCeruleanRivalPreBattleTextOffset,
             &result.pre_battle},
            {kCeruleanRivalDefeatedTextOffset,
             &result.defeated},
            {kCeruleanRivalVictoryTextOffset,
             &result.victory},
            {kCeruleanRivalAfterBattleTextOffset,
             &result.after_battle},
        }};
    for (const auto& [offset, text] : text_programs)
        if (!decode_text_program(
                rom, 0x06U, offset, *text) ||
            !text->complete || text->pages.empty()) {
            error =
                "Cerulean rival dialogue could not be decoded from the pinned ROM";
            return false;
        }
    return true;
}

std::uint32_t packed_position(std::uint8_t x, std::uint8_t y) {
    return static_cast<std::uint32_t>(x) |
           static_cast<std::uint32_t>(y) << 16U;
}

std::string source_quote(std::string_view value) {
    std::string result{"\""};
    for (const char character : value) {
        if (character == '\\')
            result += "\\\\";
        else if (character == '"')
            result += "\\\"";
        else if (character == '\n')
            result += "\\n";
        else if (character == '\r')
            result += "\\r";
        else if (character == '\t')
            result += "\\t";
        else
            result.push_back(character);
    }
    result.push_back('"');
    return result;
}

std::string page_source(const std::vector<std::string>& pages, std::string_view indentation) {
    std::ostringstream output;
    for (const std::string& page : pages)
        output << indentation << "page " << source_quote(page) << '\n';
    return output.str();
}

GeneratedFile readable_naming_profile_source(
    const NamingProfile& profile, std::string_view heading,
    const DecodedTextProgram& nickname_question) {
    std::ostringstream source;
    source
        << "; Decoded from the verified Pokemon Red US rev 0 naming tables.\n"
        << "; Cells, case actions, length, and question text remain imported content.\n\n"
        << "naming_profile pokemon_red_english\n"
        << "    maximum_length "
        << static_cast<unsigned>(profile.maximum_length) << '\n'
        << "    nickname_heading " << source_quote(heading) << '\n'
        << "    uppercase";
    for (const std::string& cell : profile.uppercase)
        source << ' ' << source_quote(cell);
    source << "\n    lowercase";
    for (const std::string& cell : profile.lowercase)
        source << ' ' << source_quote(cell);
    source << "\n    uppercase_action "
           << source_quote(profile.uppercase_action)
           << "\n    lowercase_action "
           << source_quote(profile.lowercase_action)
           << "\n    nickname_question\n"
           << page_source(nickname_question.pages, "        ");
    const std::string text = source.str();
    return {
        .relative_path = "source/menus/naming.sexpr",
        .bytes = std::vector<std::uint8_t>(text.begin(), text.end()),
    };
}

GeneratedFile readable_pallet_source(const std::vector<std::string>& hey_wait,
                                     const std::vector<std::string>& unsafe,
                                     const std::vector<PathCommand>& oak_path,
                                     const std::vector<PathCommand>& player_path,
                                     const std::vector<PathCommand>& lab_oak_path,
                                     const std::vector<PathCommand>& lab_player_path,
                                     const std::array<DecodedTextProgram, 4>& lab_choice_text) {
    const auto path_name = [](PathCommand command) {
        switch (command) {
        case PathCommand::down:
            return "down";
        case PathCommand::up:
            return "up";
        case PathCommand::left:
            return "left";
        case PathCommand::right:
            return "right";
        case PathCommand::wait:
            return "wait";
        case PathCommand::face_down:
            return "face_down";
        }
        return "invalid";
    };
    std::ostringstream source;
    source << "; Lifted from the verified Pokemon Red US rev 0 campaign program.\n"
           << "; Numeric source addresses are importer evidence, not runtime dependencies.\n\n"
           << "campaign_program pallet_oak_interception\n"
           << "    source bank_06 0x018e81\n"
           << "    trigger map pallet_town player_y 1\n"
           << "    absent_flag 0x" << std::hex << kFollowedOakFlag << std::dec << "\n"
           << "    initially_hide actor pallet_town 1\n"
           << "    initially_hide actor oaks_lab 5\n"
           << "    initially_hide actor oaks_lab 8\n"
           << "    lock_input\n"
           << "    set_flag 0x" << std::hex << kOakAppearedFlag << std::dec << "\n"
           << "    say\n"
           << page_source(hey_wait, "        ") << "    show_actor 1\n"
           << "    face_actor 1 up\n"
           << "    move_actor_to_player 1 stop_below 1\n"
           << "    face_player down\n"
           << "    say\n"
           << page_source(unsafe, "        ") << "    align_pair_x actor 1 x 10\n"
           << "    parallel_path actor 1 hide_at_end\n"
           << "        actor";
    for (const PathCommand command : oak_path)
        source << ' ' << path_name(command);
    source << "\n        player";
    for (const PathCommand command : player_path)
        source << ' ' << path_name(command);
    source << "\n"
           << "    show_actor map oaks_lab actor 8\n"
           << "    parallel_path map oaks_lab actor 8 hide_at_end\n"
           << "        actor";
    for (const PathCommand command : lab_oak_path)
        source << ' ' << path_name(command);
    source << "\n        player";
    for (std::size_t index = 0U; index < lab_oak_path.size(); ++index)
        source << " wait";
    source << "\n"
           << "    show_actor map oaks_lab actor 5\n"
           << "    face_actor map oaks_lab actor 5 down\n"
           << "    face_actor map oaks_lab actor 1 up\n"
           << "    parallel_path map oaks_lab actor 5\n"
           << "        actor";
    for (std::size_t index = 0U; index < lab_player_path.size(); ++index)
        source << " wait";
    source << "\n        player";
    for (const PathCommand command : lab_player_path)
        source << ' ' << path_name(command);
    source << "\n"
           << "    set_flag 0x" << std::hex << kFollowedOakFlag << "\n"
           << "    set_flag 0x" << kFollowedOakSecondFlag << std::dec << "\n"
           << "    say\n"
           << page_source(lab_choice_text[0].pages, "        ") << "    say\n"
           << page_source(lab_choice_text[1].pages, "        ") << "    say\n"
           << page_source(lab_choice_text[2].pages, "        ") << "    say\n"
           << page_source(lab_choice_text[3].pages, "        ") << "    set_flag 0x" << std::hex
           << kOakAskedToChooseFlag << std::dec << "\n"
           << "    unlock_input\n"
           << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path = "source/scripts/campaign/pallet_oak_interception.sexpr",
        .bytes = std::vector<std::uint8_t>(text.begin(), text.end()),
    };
}

GeneratedFile readable_starter_source(
    std::string_view key, const StarterChoice& choice,
    const DecodedTextProgram& prompt,
    const DecodedTextProgram& energetic,
    const DecodedTextProgram& player_received,
    const DecodedTextProgram& nickname_question,
    const DecodedTextProgram& rival_takes,
    const DecodedTextProgram& rival_received,
    const std::vector<PathCommand>& rival_path) {
    const auto path_name = [](PathCommand command) {
        switch (command) {
        case PathCommand::down:
            return "down";
        case PathCommand::up:
            return "up";
        case PathCommand::left:
            return "left";
        case PathCommand::right:
            return "right";
        case PathCommand::wait:
            return "wait";
        case PathCommand::face_down:
            return "face_down";
        }
        return "invalid";
    };

    std::ostringstream source;
    source << "; Lifted from the verified Pokemon Red US rev 0 starter program.\n"
           << "; Species and ball actor IDs below were decoded from the ROM program.\n\n"
           << "campaign_program " << key << '\n'
           << "    source bank_07 actor " << static_cast<unsigned>(choice.selected_ball)
           << '\n'
           << "    trigger map oaks_lab actor_activation "
           << static_cast<unsigned>(choice.selected_ball) << '\n'
           << "    required_flag 0x" << std::hex << kOakAskedToChooseFlag << '\n'
           << "    absent_flag 0x" << kGotStarterFlag << std::dec << '\n'
           << "    lock_input\n"
           << "    ask_yes_no species_dex "
           << static_cast<unsigned>(choice.player_species) << '\n'
           << page_source(prompt.pages, "        ")
           << "    end_if_choice_no\n"
           << "    hide_actor map oaks_lab actor "
           << static_cast<unsigned>(choice.selected_ball) << '\n'
           << "    say species_dex "
           << static_cast<unsigned>(choice.player_species) << '\n'
           << page_source(energetic.pages, "        ")
           << "    say species_dex "
           << static_cast<unsigned>(choice.player_species) << '\n'
           << page_source(player_received.pages, "        ")
           << "    give_pokemon species_dex "
           << static_cast<unsigned>(choice.player_species)
           << " level 5\n"
           << "    ask_yes_no species_dex "
           << static_cast<unsigned>(choice.player_species) << '\n'
           << page_source(nickname_question.pages, "        ")
           << "    nickname_last_party_member_if_yes\n"
           << "    set_variable player_starter "
           << static_cast<unsigned>(choice.player_species) << '\n'
           << "    set_variable rival_starter "
           << static_cast<unsigned>(choice.rival_species) << '\n'
           << "    parallel_path map oaks_lab actor 1\n"
           << "        actor";
    for (const PathCommand command : rival_path)
        source << ' ' << path_name(command);
    source << "\n        player";
    for (std::size_t index = 0U; index < rival_path.size(); ++index)
        source << " wait";
    source << "\n"
           << "    face_actor map oaks_lab actor 1 up\n"
           << "    say\n"
           << page_source(rival_takes.pages, "        ")
           << "    hide_actor map oaks_lab actor "
           << static_cast<unsigned>(choice.rival_ball) << '\n'
           << "    say species_dex "
           << static_cast<unsigned>(choice.rival_species) << '\n'
           << page_source(rival_received.pages, "        ")
           << "    set_flag 0x" << std::hex << kGotStarterFlag << std::dec << '\n'
           << "    unlock_input\n"
           << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/" + std::string(key) + ".sexpr",
        .bytes = std::vector<std::uint8_t>(text.begin(), text.end()),
    };
}

GeneratedFile readable_rival_battle_source(
    std::string_view key, const RivalBattleChoice& battle,
    const DecodedTextProgram& challenge,
    const DecodedTextProgram& player_victory,
    const DecodedTextProgram& player_loss,
    const DecodedTextProgram& exit_text,
    const std::vector<PathCommand>& exit_path) {
    const auto path_name = [](PathCommand command) {
        switch (command) {
        case PathCommand::down:
            return "down";
        case PathCommand::up:
            return "up";
        case PathCommand::left:
            return "left";
        case PathCommand::right:
            return "right";
        case PathCommand::wait:
            return "wait";
        case PathCommand::face_down:
            return "face_down";
        }
        return "invalid";
    };

    std::ostringstream source;
    source << "; Lifted from the verified Pokemon Red US rev 0 lab-rival program.\n"
           << "; Trainer class, party, text, flags, and movement are ROM-derived.\n\n"
           << "campaign_program " << key << '\n'
           << "    source bank_07 0x01cdb9\n"
           << "    trigger map oaks_lab player_y 6\n"
           << "    required_flag 0x" << std::hex << kGotStarterFlag << '\n'
           << "    absent_flag 0x" << kBattledLabRivalFlag << std::dec << '\n'
           << "    required_variable rival_starter "
           << static_cast<unsigned>(battle.rival_species) << '\n'
           << "    lock_input\n"
           << "    face_actor map oaks_lab actor 1 down\n"
           << "    face_player up\n"
           << "    say\n"
           << page_source(challenge.pages, "        ")
           << "    move_actor_to_player map oaks_lab actor 1 stop_above 1\n"
           << "    start_trainer_battle class "
           << static_cast<unsigned>(battle.trainer_class)
           << " party " << battle.trainer_party << '\n'
           << "    say_if_player_won\n"
           << page_source(player_victory.pages, "        ")
           << "    say_if_player_lost\n"
           << page_source(player_loss.pages, "        ")
           << "    face_actor map oaks_lab actor 1 down\n"
           << "    heal_party\n"
           << "    set_flag 0x" << std::hex << kBattledLabRivalFlag << std::dec << '\n'
           << "    wait_ticks 20\n"
           << "    say\n"
           << page_source(exit_text.pages, "        ")
           << "    actor_path_by_player_x map oaks_lab actor 1 equals 4\n"
           << "        equal";
    for (const PathCommand command : exit_path)
        source << ' ' << path_name(command);
    source << " right\n"
           << "        otherwise";
    for (const PathCommand command : exit_path)
        source << ' ' << path_name(command);
    source << " left\n"
           << "    hide_actor map oaks_lab actor 1\n"
           << "    unlock_input\n"
           << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/" + std::string(key) + ".sexpr",
        .bytes = std::vector<std::uint8_t>(text.begin(), text.end()),
    };
}

GeneratedFile readable_viridian_mart_parcel_source(
    const ParcelProgram& parcel) {
    const auto path_name = [](PathCommand command) {
        switch (command) {
        case PathCommand::down:
            return "down";
        case PathCommand::up:
            return "up";
        case PathCommand::left:
            return "left";
        case PathCommand::right:
            return "right";
        case PathCommand::wait:
            return "wait";
        case PathCommand::face_down:
            return "face_down";
        }
        return "invalid";
    };

    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Viridian Mart program.\n"
        << "; The trigger, event flags, path, item grant, and text are ROM-derived.\n"
        << "; The source switches clerk tables after flag 0x" << std::hex
        << parcel.oak_got_parcel_flag << std::dec << ".\n\n"
        << "campaign_program viridian_mart_oaks_parcel\n"
        << "    source bank_07 0x01d49b\n"
        << "    trigger map viridian_mart map_entry\n"
        << "    absent_flag 0x" << std::hex
        << parcel.got_oaks_parcel_flag << std::dec << '\n'
        << "    lock_input\n"
        << "    say\n"
        << page_source(parcel.came_from_pallet.pages, "        ")
        << "    player_path";
    for (const PathCommand command : parcel.player_path)
        source << ' ' << path_name(command);
    source
        << "\n"
        << "    wait_ticks 3\n"
        << "    say\n"
        << page_source(parcel.parcel_quest.pages, "        ")
        << "    give_item oaks_parcel quantity "
        << static_cast<unsigned>(parcel.quantity)
        << " rom_id " << parcel.item_id << '\n'
        << "    set_flag 0x" << std::hex
        << parcel.got_oaks_parcel_flag << std::dec << '\n'
        << "    unlock_input\n"
        << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/viridian_mart_oaks_parcel.sexpr",
        .bytes = std::vector<std::uint8_t>(text.begin(), text.end()),
    };
}

GeneratedFile readable_initial_actor_visibility_source(
    const std::vector<ToggleActor>& actors) {
    std::ostringstream source;
    source
        << "; Decoded from the verified cartridge toggleable-object table.\n"
        << "; Map and actor IDs retain provenance until the shared naming pass owns this file.\n\n"
        << "initial_actor_visibility pokemon_red\n";
    for (std::size_t index = 0U; index < actors.size(); ++index) {
        const ToggleActor& actor = actors[index];
        source << "    actor map_" << static_cast<unsigned>(actor.map_id)
               << ' ' << static_cast<unsigned>(actor.actor_index)
               << ' '
               << (actor.initially_visible ? "visible" : "hidden")
               << " rom_toggle_index " << index << '\n';
    }
    const std::string text = source.str();
    return {
        .relative_path =
            "source/world/initial_actor_visibility.sexpr",
        .bytes = std::vector<std::uint8_t>(text.begin(), text.end()),
    };
}

GeneratedFile readable_oak_return_source(
    const OakReturnProgram& request) {
    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Oak request program.\n"
        << "; Inventory, flags, toggle actors, placement branches, paths, and text are ROM-derived.\n\n"
        << "campaign_program oaks_lab_deliver_parcel_and_get_pokedex\n"
        << "    source bank_07 0x01d2a9\n"
        << "    trigger map oaks_lab actor_activation 5\n"
        << "    required_flag 0x" << std::hex
        << kBattledLabRivalFlag << '\n'
        << "    absent_flag 0x" << request.oak_got_parcel_flag
        << std::dec << '\n'
        << "    required_item oaks_parcel quantity "
        << static_cast<unsigned>(request.parcel_quantity)
        << " rom_id " << request.parcel_item_id << '\n'
        << "    lock_input\n"
        << "    say\n"
        << page_source(request.deliver_parcel.pages, "        ")
        << "    take_item oaks_parcel quantity "
        << static_cast<unsigned>(request.parcel_quantity)
        << " rom_id " << request.parcel_item_id << '\n'
        << "    say\n"
        << page_source(request.rival_gramps.pages, "        ")
        << "    choose_path player_y\n";
    for (std::size_t index = 0U;
         index < request.movement_steps.size(); ++index) {
        source
            << "        when "
            << (index < 2U
                    ? std::to_string(request.player_y[index])
                    : "otherwise")
            << " place_actor map oaks_lab actor 1 at "
            << static_cast<unsigned>(request.spawn_x) << ' '
            << static_cast<unsigned>(request.spawn_y[index])
            << " show_then";
        for (std::uint8_t step = 0U;
             step < request.movement_steps[index]; ++step)
            source << " up";
        source << '\n';
    }
    source
        << "    face_actor map oaks_lab actor 1 up\n"
        << "    face_actor map oaks_lab actor 5 down\n"
        << "    say\n"
        << page_source(
               request.rival_what_did_you_call.pages, "        ")
        << "    wait_ticks 1\n"
        << "    say\n"
        << page_source(request.oak_request.pages, "        ")
        << "    wait_ticks 1\n"
        << "    say\n"
        << page_source(
               request.pokedex_invention.pages, "        ")
        << "    wait_ticks 1\n"
        << "    say\n"
        << page_source(request.got_pokedex.pages, "        ")
        << "    wait_ticks 3\n"
        << "    hide_actor map_"
        << static_cast<unsigned>(request.pokedex_1.map_id)
        << " actor "
        << static_cast<unsigned>(request.pokedex_1.actor_index)
        << "\n"
        << "    hide_actor map_"
        << static_cast<unsigned>(request.pokedex_2.map_id)
        << " actor "
        << static_cast<unsigned>(request.pokedex_2.actor_index)
        << "\n"
        << "    say\n"
        << page_source(request.oak_dream.pages, "        ")
        << "    face_actor map oaks_lab actor 1 right\n"
        << "    wait_ticks 3\n"
        << "    say\n"
        << page_source(request.rival_leave.pages, "        ")
        << "    set_flag 0x" << std::hex
        << request.got_pokedex_flag << '\n'
        << "    set_flag 0x" << request.oak_got_parcel_flag
        << std::dec << '\n'
        << "    hide_actor map_"
        << static_cast<unsigned>(request.lying_old_man.map_id)
        << " actor "
        << static_cast<unsigned>(request.lying_old_man.actor_index)
        << "\n"
        << "    show_actor map_"
        << static_cast<unsigned>(request.standing_old_man.map_id)
        << " actor "
        << static_cast<unsigned>(request.standing_old_man.actor_index)
        << "\n"
        << "    reverse_selected_path_and_hide actor 1\n"
        << "    set_flag 0x" << std::hex
        << request.first_route_22_rival_flag << '\n'
        << "    clear_flag 0x"
        << request.second_route_22_rival_flag << '\n'
        << "    set_flag 0x"
        << request.route_22_rival_wants_battle_flag
        << std::dec << '\n'
        << "    show_actor map_"
        << static_cast<unsigned>(request.route_22_rival.map_id)
        << " actor "
        << static_cast<unsigned>(request.route_22_rival.actor_index)
        << "\n"
        << "    unlock_input\n"
        << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/oaks_lab_deliver_parcel_and_get_pokedex.sexpr",
        .bytes = std::vector<std::uint8_t>(text.begin(), text.end()),
    };
}

GeneratedFile readable_route_22_first_rival_source(
    std::string_view key, const Route22FirstRivalProgram& route,
    const RivalBattleChoice& battle) {
    const auto path_name = [](PathCommand command) {
        switch (command) {
        case PathCommand::down:
            return "down";
        case PathCommand::up:
            return "up";
        case PathCommand::left:
            return "left";
        case PathCommand::right:
            return "right";
        case PathCommand::wait:
            return "wait";
        case PathCommand::face_down:
            return "face_down";
        }
        return "invalid";
    };
    const auto append_path =
        [&](std::ostringstream& output,
            const std::vector<PathCommand>& path) {
            for (const PathCommand command : path)
                output << ' ' << path_name(command);
        };

    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Route 22 program.\n"
        << "; Trigger, paths, starter branch, trainer party, text, actors, and flags are ROM-derived.\n\n"
        << "campaign_program " << key << '\n'
        << "    source bank_14 0x050f00\n"
        << "    trigger_rectangle map route_22 x "
        << static_cast<unsigned>(route.trigger_x) << " y "
        << static_cast<unsigned>(route.trigger_y)
        << " width "
        << static_cast<unsigned>(route.trigger_width)
        << " height "
        << static_cast<unsigned>(route.trigger_height) << '\n'
        << "    required_flag 0x" << std::hex
        << route.wants_battle_flag << '\n'
        << "    absent_flag 0x" << route.beat_rival_flag
        << std::dec << '\n'
        << "    required_variable rival_starter "
        << static_cast<unsigned>(battle.rival_species) << '\n'
        << "    lock_input\n"
        << "    actor_path_by_player_y map route_22 actor "
        << static_cast<unsigned>(route.actor_index)
        << " equals "
        << static_cast<unsigned>(
               route.trigger_y + route.trigger_height - 1U)
        << "\n"
        << "        equal";
    append_path(source, route.approach_lower);
    source << "\n        otherwise";
    append_path(source, route.approach_upper);
    source
        << "\n"
        << "    face_pair_by_player_y lower horizontal otherwise vertical\n"
        << "    say\n"
        << page_source(route.before_battle.pages, "        ")
        << "    start_trainer_battle class "
        << static_cast<unsigned>(battle.trainer_class)
        << " party " << battle.trainer_party << '\n'
        << "    say_if_player_won\n"
        << page_source(route.defeated.pages, "        ")
        << "    say_if_player_lost\n"
        << page_source(route.victory.pages, "        ")
        << "    end_if_player_lost\n"
        << "    set_flag 0x" << std::hex
        << route.beat_rival_flag << std::dec << '\n'
        << "    say\n"
        << page_source(route.after_battle.pages, "        ")
        << "    actor_path_by_player_y map route_22 actor "
        << static_cast<unsigned>(route.actor_index)
        << " equals "
        << static_cast<unsigned>(
               route.trigger_y + route.trigger_height - 1U)
        << "\n"
        << "        equal";
    append_path(source, route.exit_lower);
    source << "\n        otherwise";
    append_path(source, route.exit_upper);
    source
        << "\n"
        << "    hide_actor map route_22 actor "
        << static_cast<unsigned>(route.actor_index) << '\n'
        << "    clear_flag 0x" << std::hex
        << route.first_rival_flag << '\n'
        << "    clear_flag 0x" << route.wants_battle_flag
        << std::dec << '\n'
        << "    unlock_input\n"
        << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/" + std::string(key) +
            ".sexpr",
        .bytes = std::vector<std::uint8_t>(
            text.begin(), text.end()),
    };
}

GeneratedFile readable_oak_pokeball_source(
    const OakPokeballProgram& oak) {
    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Oak text program.\n"
        << "; Gate, check-and-set event, item grant, quantity, and dialogue are ROM-derived.\n\n"
        << "campaign_program oaks_lab_give_pokeballs\n"
        << "    source bank_07 0x01d280\n"
        << "    trigger map oaks_lab actor_activation 5\n"
        << "    required_flag 0x" << std::hex
        << oak.beat_route_22_rival_flag << '\n'
        << "    absent_flag 0x" << oak.got_pokeballs_flag
        << std::dec << '\n'
        << "    lock_input\n"
        << "    set_flag 0x" << std::hex
        << oak.got_pokeballs_flag << std::dec << '\n'
        << "    try_give_item rom_id " << oak.item_id
        << " quantity "
        << static_cast<unsigned>(oak.quantity) << '\n'
        << "    say\n"
        << page_source(oak.give_pokeballs.pages, "        ")
        << "    unlock_input\n"
        << "    end\n\n"
        << "campaign_program oaks_lab_after_pokeballs\n"
        << "    trigger map oaks_lab actor_activation 5\n"
        << "    required_flag 0x" << std::hex
        << oak.got_pokeballs_flag << std::dec << '\n'
        << "    lock_input\n"
        << "    say\n"
        << page_source(oak.come_see_me.pages, "        ")
        << "    unlock_input\n"
        << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/oaks_lab_pokeballs.sexpr",
        .bytes = std::vector<std::uint8_t>(
            text.begin(), text.end()),
    };
}

GeneratedFile readable_daisy_town_map_source(
    const DaisyTownMapProgram& daisy) {
    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Blue's House program.\n"
        << "; Predicates, capacity branch, item, actor toggle, event, and dialogue are ROM-derived.\n\n"
        << "campaign_program blues_house_daisy_before_pokedex\n"
        << "    source bank_06 0x019b5d\n"
        << "    trigger map blues_house actor_activation 1\n"
        << "    absent_flag 0x" << std::hex
        << daisy.got_pokedex_flag << std::dec << '\n'
        << "    lock_input\n"
        << "    say\n"
        << page_source(daisy.rival_at_lab.pages, "        ")
        << "    unlock_input\n"
        << "    end\n\n"
        << "campaign_program blues_house_daisy_give_town_map\n"
        << "    trigger map blues_house actor_activation 1\n"
        << "    required_flag 0x" << std::hex
        << daisy.got_pokedex_flag << '\n'
        << "    absent_flag 0x" << daisy.got_town_map_flag
        << std::dec << '\n'
        << "    lock_input\n"
        << "    say\n"
        << page_source(daisy.offer_map.pages, "        ")
        << "    try_give_item " << source_quote(daisy.item_name)
        << " rom_id " << daisy.item_id << " quantity "
        << static_cast<unsigned>(daisy.quantity) << '\n'
        << "    if_item_grant_failed\n"
        << "        say\n"
        << page_source(daisy.bag_full.pages, "            ")
        << "        unlock_input\n"
        << "        end\n"
        << "    hide_actor map blues_house actor "
        << static_cast<unsigned>(
               daisy.town_map_actor.actor_index)
        << '\n'
        << "    say\n"
        << page_source(daisy.got_map.pages, "        ")
        << "    set_flag 0x" << std::hex
        << daisy.got_town_map_flag << std::dec << '\n'
        << "    unlock_input\n"
        << "    end\n\n"
        << "campaign_program blues_house_daisy_after_town_map\n"
        << "    trigger map blues_house actor_activation 1\n"
        << "    required_flag 0x" << std::hex
        << daisy.got_town_map_flag << std::dec << '\n'
        << "    lock_input\n"
        << "    say\n"
        << page_source(daisy.use_map.pages, "        ")
        << "    unlock_input\n"
        << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/blues_house_daisy_town_map.sexpr",
        .bytes = std::vector<std::uint8_t>(
            text.begin(), text.end()),
    };
}

GeneratedFile readable_pallet_reward_updates_source(
    const PalletRewardUpdates& updates) {
    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Pallet and Blue's House map programs.\n"
        << "; These guarded map-presence updates preserve source flags and Daisy's actor transition.\n\n"
        << "campaign_program blues_house_record_entry\n"
        << "    trigger map blues_house map_presence\n"
        << "    absent_flag 0x" << std::hex
        << updates.entered_blues_house_flag << '\n'
        << "    set_flag 0x"
        << updates.entered_blues_house_flag << "\n    end\n\n"
        << "campaign_program pallet_after_pokeballs\n"
        << "    trigger map pallet_town map_presence\n"
        << "    required_flag 0x"
        << updates.got_pokeballs_flag << '\n'
        << "    absent_flag 0x"
        << updates.pallet_after_pokeballs_flag << '\n'
        << "    set_flag 0x"
        << updates.pallet_after_pokeballs_flag
        << "\n    end\n\n"
        << "campaign_program pallet_daisy_starts_walking\n"
        << "    trigger map pallet_town map_presence\n"
        << "    required_flag 0x"
        << updates.got_town_map_flag << '\n'
        << "    absent_flag 0x"
        << updates.daisy_walking_flag << '\n'
        << "    set_flag 0x" << updates.daisy_walking_flag
        << std::dec << '\n'
        << "    hide_actor map_"
        << static_cast<unsigned>(updates.daisy_sitting.map_id)
        << " actor "
        << static_cast<unsigned>(updates.daisy_sitting.actor_index)
        << '\n'
        << "    show_actor map_"
        << static_cast<unsigned>(updates.daisy_walking.map_id)
        << " actor "
        << static_cast<unsigned>(updates.daisy_walking.actor_index)
        << "\n    end\n\n"
        << "campaign_program pallet_after_pokeballs_2\n"
        << "    trigger map pallet_town map_presence\n"
        << "    required_flag 0x" << std::hex
        << updates.got_pokeballs_flag << '\n'
        << "    absent_flag 0x"
        << updates.pallet_after_pokeballs_2_flag << '\n'
        << "    set_flag 0x"
        << updates.pallet_after_pokeballs_2_flag
        << std::dec << "\n    end\n";
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/pallet_reward_updates.sexpr",
        .bytes = std::vector<std::uint8_t>(
            text.begin(), text.end()),
    };
}

GeneratedFile readable_route_1_potion_source(
    const Route1PotionProgram& potion) {
    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Route 1 actor program.\n"
        << "; Check-and-set event, item tuple, capacity branch, name, and text are ROM-derived.\n\n"
        << "campaign_program route_1_potion_sample\n"
        << "    source bank_07 0x01cab9\n"
        << "    trigger map route_1 actor_activation 1\n"
        << "    absent_flag 0x" << std::hex
        << potion.got_potion_sample_flag << std::dec << '\n'
        << "    lock_input\n"
        << "    set_flag 0x" << std::hex
        << potion.got_potion_sample_flag << std::dec << '\n'
        << "    say\n"
        << page_source(potion.sample.pages, "        ")
        << "    try_give_item " << source_quote(potion.item_name)
        << " rom_id " << potion.item_id << " quantity "
        << static_cast<unsigned>(potion.quantity) << '\n'
        << "    if_item_grant_failed\n"
        << "        say\n"
        << page_source(potion.no_room.pages, "            ")
        << "        unlock_input\n"
        << "        end\n"
        << "    say\n"
        << page_source(potion.got_item.pages, "        ")
        << "    unlock_input\n"
        << "    end\n\n"
        << "campaign_program route_1_after_potion_sample\n"
        << "    trigger map route_1 actor_activation 1\n"
        << "    required_flag 0x" << std::hex
        << potion.got_potion_sample_flag << std::dec << '\n'
        << "    lock_input\n"
        << "    say\n"
        << page_source(potion.repeat.pages, "        ")
        << "    unlock_input\n"
        << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/route_1_potion.sexpr",
        .bytes = std::vector<std::uint8_t>(
            text.begin(), text.end()),
    };
}

GeneratedFile readable_loose_item_source(
    const std::vector<ImportedItemName>& item_names,
    const DecodedTextProgram& found,
    const DecodedTextProgram& no_room) {
    std::ostringstream source;
    source
        << "; Generic loose-item semantics assembled from verified Pokemon Red US rev 0 data.\n"
        << "; Actor item IDs come from each imported map object; names and both result texts come from ROM tables.\n\n"
        << "loose_item_pickup\n"
        << "    item_id actor_parameter_a\n"
        << "    quantity 1\n"
        << "    if_inventory_accepts\n"
        << "        hide_activated_actor\n"
        << "        say\n"
        << page_source(found.pages, "            ")
        << "    else\n"
        << "        say\n"
        << page_source(no_room.pages, "            ")
        << "\nitem_names\n";
    for (const ImportedItemName& item : item_names)
        source << "    item " << item.item_id << ' '
               << source_quote(item.name) << '\n';
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/loose_item_pickup.sexpr",
        .bytes = std::vector<std::uint8_t>(
            text.begin(), text.end()),
    };
}

GeneratedFile readable_pewter_city_source(
    const PewterCityProgram& city) {
    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Pewter City program.\n"
        << "; Actor origins, gate cells, destinations, flags, and dialogue are ROM-derived.\n"
        << "; escort_player_to is a modern semantic lowering over the imported map collision graph.\n\n"
        << "campaign_program pewter_city_museum_guide\n"
        << "    trigger map pewter_city actor_activation "
        << static_cast<unsigned>(city.museum_actor_index) << '\n'
        << "    ask_yes_no\n"
        << page_source(city.museum_question.pages, "        ")
        << "    if_yes\n"
        << "        say\n"
        << page_source(city.museum_yes.pages, "            ")
        << "    if_no\n"
        << "        say\n"
        << page_source(city.museum_no.pages, "            ")
        << "        escort_player_to "
        << static_cast<unsigned>(city.museum_player_target.x)
        << ' '
        << static_cast<unsigned>(city.museum_player_target.y)
        << " actor_side left\n"
        << "        say\n"
        << page_source(city.museum_arrival.pages, "            ")
        << "        move_actor down 4 hide_at_end\n"
        << "        restore_actor "
        << static_cast<unsigned>(city.museum_actor_origin.x)
        << ' '
        << static_cast<unsigned>(city.museum_actor_origin.y)
        << "\n\n"
        << "campaign_program pewter_city_repel_explanation\n"
        << "    trigger map pewter_city actor_activation "
        << static_cast<unsigned>(city.repel_actor_index) << '\n'
        << "    ask_yes_no\n"
        << page_source(city.repel_question.pages, "        ")
        << "    if_yes\n"
        << "        say\n"
        << page_source(city.repel_yes.pages, "            ")
        << "    if_no\n"
        << "        say\n"
        << page_source(city.repel_no.pages, "            ")
        << '\n'
        << "campaign_template pewter_city_gym_escort\n"
        << "    absent_flag 0x" << std::hex
        << city.beat_brock_flag << std::dec << '\n'
        << "    say\n"
        << page_source(city.gym_follow.pages, "        ")
        << "    escort_player_to "
        << static_cast<unsigned>(city.gym_player_target.x)
        << ' '
        << static_cast<unsigned>(city.gym_player_target.y)
        << " actor_side right\n"
        << "    say\n"
        << page_source(city.gym_arrival.pages, "        ")
        << "    move_actor right 5 hide_at_end\n"
        << "    restore_actor "
        << static_cast<unsigned>(city.gym_guide_actor_origin.x)
        << ' '
        << static_cast<unsigned>(city.gym_guide_actor_origin.y)
        << "\n    triggers\n"
        << "        actor_activation "
        << static_cast<unsigned>(city.gym_guide_actor_index)
        << '\n';
    for (const PewterCityProgram::Position& cell :
         city.east_gate_cells)
        source << "        player_cell "
               << static_cast<unsigned>(cell.x) << ' '
               << static_cast<unsigned>(cell.y) << '\n';
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/pewter_city.sexpr",
        .bytes = std::vector<std::uint8_t>(
            text.begin(), text.end()),
    };
}

GeneratedFile readable_mt_moon_fossil_source(
    const MtMoonFossilProgram& fossil) {
    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Mt. Moon B2F program.\n"
        << "; Trainer binding, flags, item tuples, actor toggles, movement, and text are ROM-derived.\n\n"
        << "campaign_program mt_moon_b2f_super_nerd_battle\n"
        << "    triggers\n"
        << "        actor_activation "
        << static_cast<unsigned>(fossil.super_nerd_actor_index)
        << "\n        player_cell "
        << static_cast<unsigned>(fossil.trigger_x) << ' '
        << static_cast<unsigned>(fossil.trigger_y) << '\n'
        << "    absent_flag 0x" << std::hex
        << fossil.beat_super_nerd_flag << std::dec << '\n'
        << "    after_victory suppress_wild_encounters rectangle "
        << static_cast<unsigned>(fossil.suppression_x) << ' '
        << static_cast<unsigned>(fossil.suppression_y) << ' '
        << static_cast<unsigned>(fossil.suppression_width) << ' '
        << static_cast<unsigned>(fossil.suppression_height)
        << '\n'
        << "    say\n"
        << page_source(fossil.both_mine.pages, "        ")
        << "    start_trainer_battle class "
        << static_cast<unsigned>(fossil.trainer_class)
        << " party " << fossil.trainer_party << '\n'
        << "    if_player_won\n"
        << "        say\n"
        << page_source(fossil.okay_share.pages, "            ")
        << "        set_flag 0x" << std::hex
        << fossil.beat_super_nerd_flag << std::dec << '\n'
        << "\ninteraction super_nerd_before_choice\n"
        << "    required_flag 0x" << std::hex
        << fossil.beat_super_nerd_flag << std::dec << '\n'
        << "    say\n"
        << page_source(fossil.each_take_one.pages, "        ")
        << "\ninteraction super_nerd_after_choice\n"
        << "    required_any_flag 0x" << std::hex
        << fossil.got_dome_flag << " 0x"
        << fossil.got_helix_flag << std::dec << '\n'
        << "    say\n"
        << page_source(fossil.pokemon_lab.pages, "        ");

    const auto write_choice =
        [&](std::string_view key, std::uint8_t actor,
            std::uint16_t item_id, std::uint8_t quantity,
            std::uint32_t got_flag, std::string_view item_name,
            const DecodedTextProgram& question,
            const DecodedTextProgram& received,
            std::string_view nerd_step) {
            source
                << "\ncampaign_program " << key << '\n'
                << "    trigger actor_activation "
                << static_cast<unsigned>(actor) << '\n'
                << "    required_flag 0x" << std::hex
                << fossil.beat_super_nerd_flag << std::dec << '\n'
                << "    ask_yes_no\n"
                << page_source(question.pages, "        ")
                << "    if_yes\n"
                << "        try_give_item "
                << source_quote(item_name) << " rom_id "
                << item_id << " quantity "
                << static_cast<unsigned>(quantity) << '\n'
                << "        if_item_grant_failed\n"
                << "            say\n"
                << page_source(fossil.no_room.pages, "                ")
                << "        else\n"
                << "            say\n"
                << page_source(received.pages, "                ")
                << "            set_flag 0x" << std::hex
                << got_flag << std::dec << '\n'
                << "            move_actor super_nerd "
                << nerd_step << " 1 scripted\n"
                << "            say\n"
                << page_source(fossil.then_mine.pages, "                ")
                << "            hide_both_fossil_actors\n";
        };
    write_choice(
        "mt_moon_b2f_take_dome_fossil",
        fossil.dome_actor_index, fossil.dome_item_id,
        fossil.dome_quantity, fossil.got_dome_flag,
        fossil.dome_item_name, fossil.dome_question,
        fossil.dome_received, "right");
    write_choice(
        "mt_moon_b2f_take_helix_fossil",
        fossil.helix_actor_index, fossil.helix_item_id,
        fossil.helix_quantity, fossil.got_helix_flag,
        fossil.helix_item_name, fossil.helix_question,
        fossil.helix_received, "up");
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/mt_moon_fossils.sexpr",
        .bytes = std::vector<std::uint8_t>(
            text.begin(), text.end()),
    };
}

GeneratedFile readable_mt_moon_magikarp_source(
    const MtMoonMagikarpProgram& sale) {
    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Mt. Moon Pokecenter program.\n"
        << "; Owner, event, BCD price, Pokemon tuple, storage results, and dialogue are ROM-derived.\n\n"
        << "campaign_program mt_moon_pokecenter_magikarp_sale\n"
        << "    trigger map mt_moon_pokecenter actor_activation "
        << static_cast<unsigned>(sale.actor_index) << '\n'
        << "    absent_flag 0x" << std::hex
        << sale.bought_flag << std::dec << '\n'
        << "    ask_yes_no\n"
        << page_source(sale.offer.pages, "        ")
        << "    if_no\n"
        << "        say\n"
        << page_source(sale.chose_no.pages, "            ")
        << "    if_yes\n"
        << "        require_money " << sale.price << '\n'
        << "        if_insufficient_money\n"
        << "            say\n"
        << page_source(sale.no_money.pages, "                ")
        << "        else\n"
        << "            try_give_pokemon dex "
        << static_cast<unsigned>(sale.species_dex)
        << " level " << static_cast<unsigned>(sale.level) << '\n'
        << "            if_storage_full\n"
        << "                say\n"
        << page_source(sale.box_full.pages, "                    ")
        << "            else\n"
        << "                say\n"
        << page_source(sale.got_pokemon.pages, "                    ")
        << "                if_sent_to_box say\n"
        << page_source(sale.sent_to_box.pages, "                    ")
        << "                take_money " << sale.price << '\n'
        << "                set_flag 0x" << std::hex
        << sale.bought_flag << std::dec << "\n\n"
        << "campaign_program mt_moon_pokecenter_magikarp_no_refunds\n"
        << "    trigger map mt_moon_pokecenter actor_activation "
        << static_cast<unsigned>(sale.actor_index) << '\n'
        << "    required_flag 0x" << std::hex
        << sale.bought_flag << std::dec << '\n'
        << "    say\n"
        << page_source(sale.no_refunds.pages, "        ");
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/mt_moon_magikarp_sale.sexpr",
        .bytes = std::vector<std::uint8_t>(
            text.begin(), text.end()),
    };
}

GeneratedFile readable_cerulean_rival_source(
    const CeruleanRivalProgram& rival) {
    const auto path_name = [](PathCommand command) {
        switch (command) {
        case PathCommand::down: return "down";
        case PathCommand::up: return "up";
        case PathCommand::left: return "left";
        case PathCommand::right: return "right";
        case PathCommand::wait: return "wait";
        case PathCommand::face_down: return "face_down";
        }
        return "unknown";
    };
    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Cerulean City rival program.\n"
        << "; Trigger cells, hidden actor, movement, event, starter branches, parties, and text are ROM-derived.\n\n";
    for (const RivalBattleChoice& battle :
         rival.battles) {
        source
            << "campaign_program cerulean_rival_for_starter_"
            << static_cast<unsigned>(battle.rival_species)
            << "\n    trigger map cerulean_city rectangle "
            << static_cast<unsigned>(rival.trigger_x) << ' '
            << static_cast<unsigned>(rival.trigger_y) << ' '
            << static_cast<unsigned>(rival.trigger_width)
            << " 1\n    absent_flag 0x" << std::hex
            << rival.beat_flag << std::dec
            << "\n    required_variable rival_starter "
            << static_cast<unsigned>(battle.rival_species)
            << "\n    place_actor rival player_x "
            << static_cast<unsigned>(rival.approach_start_y)
            << "\n    show_actor rival\n"
            << "    move_actor rival";
        for (const PathCommand command : rival.approach)
            source << ' ' << path_name(command);
        source
            << " scripted\n    face_player rival\n"
            << "    say\n"
            << page_source(rival.pre_battle.pages, "        ")
            << "    start_trainer_battle class "
            << static_cast<unsigned>(battle.trainer_class)
            << " party " << battle.trainer_party << '\n'
            << "    if_player_won say\n"
            << page_source(rival.defeated.pages, "        ")
            << "    if_player_lost say\n"
            << page_source(rival.victory.pages, "        ")
            << "    if_player_lost hide_actor rival and_end\n"
            << "    set_flag 0x" << std::hex
            << rival.beat_flag << std::dec
            << "\n    say\n"
            << page_source(rival.after_battle.pages, "        ")
            << "    move_actor_by_player_x rival equals "
            << static_cast<unsigned>(rival.trigger_x)
            << "\n        if_equal";
        for (const PathCommand command : rival.exit_right)
            source << ' ' << path_name(command);
        source << "\n        otherwise";
        for (const PathCommand command : rival.exit_left)
            source << ' ' << path_name(command);
        source << "\n    hide_actor rival\n\n";
    }
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/cerulean_rival.sexpr",
        .bytes = std::vector<std::uint8_t>(
            text.begin(), text.end()),
    };
}

GeneratedFile readable_pewter_gym_source(
    const PewterGymProgram& gym) {
    std::ostringstream source;
    source
        << "; Lifted from the verified Pokemon Red US rev 0 Pewter Gym program.\n"
        << "; Trainer owner, reward branches, flags, item, toggles, and every page below are ROM-derived.\n\n"
        << "campaign_program pewter_gym_brock_battle\n"
        << "    trigger map pewter_gym actor_activation "
        << static_cast<unsigned>(gym.brock_actor_index) << '\n'
        << "    absent_flag 0x" << std::hex
        << gym.beat_brock_flag << std::dec << '\n'
        << "    lock_input\n"
        << "    say\n"
        << page_source(gym.pre_battle.pages, "        ")
        << "    start_trainer_battle class "
        << static_cast<unsigned>(gym.trainer_class)
        << " party " << gym.trainer_party << '\n'
        << "    if_player_won say\n"
        << page_source(gym.badge.pages, "        ")
        << "    if_player_lost end\n"
        << "    continue pewter_gym_brock_reward\n\n"
        << "campaign_program pewter_gym_brock_tm_retry\n"
        << "    trigger map pewter_gym actor_activation "
        << static_cast<unsigned>(gym.brock_actor_index) << '\n'
        << "    required_flag 0x" << std::hex
        << gym.beat_brock_flag << '\n'
        << "    absent_flag 0x" << gym.got_tm_flag
        << std::dec << '\n'
        << "    continue pewter_gym_brock_reward\n\n"
        << "campaign_fragment pewter_gym_brock_reward\n"
        << "    say\n"
        << page_source(gym.wait_take_this.pages, "        ")
        << "    set_flag 0x" << std::hex
        << gym.beat_brock_flag << std::dec << '\n'
        << "    try_give_item " << source_quote(gym.tm_name)
        << " rom_id " << gym.tm_item_id << " quantity "
        << static_cast<unsigned>(gym.tm_quantity) << '\n'
        << "    if_item_grant_failed say\n"
        << page_source(gym.no_room.pages, "        ")
        << "    else\n"
        << "        say\n"
        << page_source(gym.received_tm.pages, "            ")
        << "        set_flag 0x" << std::hex
        << gym.got_tm_flag << std::dec << '\n'
        << "    set_flag obtained_badge 0x" << std::hex
        << gym.obtained_badge_flag << '\n'
        << "    set_flag beat_gym 0x"
        << gym.beat_gym_flag << '\n'
        << "    hide_actor map_" << std::dec
        << static_cast<unsigned>(gym.gym_guide.map_id)
        << " actor "
        << static_cast<unsigned>(gym.gym_guide.actor_index)
        << '\n'
        << "    hide_actor map_"
        << static_cast<unsigned>(gym.route_rival.map_id)
        << " actor "
        << static_cast<unsigned>(gym.route_rival.actor_index)
        << '\n'
        << "    clear_flag 0x" << std::hex
        << gym.first_route_rival_flag << '\n'
        << "    clear_flag 0x"
        << gym.route_rival_wants_battle_flag << '\n'
        << "    set_flag 0x"
        << gym.gym_trainer_defeated_flag << std::dec
        << "\n    unlock_input\n    end\n\n"
        << "campaign_program pewter_gym_brock_advice\n"
        << "    required_flag 0x" << std::hex
        << gym.got_tm_flag << std::dec << '\n'
        << "    say\n"
        << page_source(gym.post_battle_advice.pages, "        ")
        << "\ncampaign_program pewter_gym_guide_before_brock\n"
        << "    absent_flag 0x" << std::hex
        << gym.beat_gym_flag << std::dec << '\n'
        << "    ask_yes_no\n"
        << page_source(gym.guide_pre_advice.pages, "        ")
        << "    if_yes say\n"
        << page_source(gym.guide_begin_advice.pages, "        ")
        << "    if_no say\n"
        << page_source(gym.guide_free_service.pages, "        ")
        << "    say\n"
        << page_source(gym.guide_advice.pages, "        ")
        << "\ncampaign_program pewter_gym_guide_after_brock\n"
        << "    required_flag 0x" << std::hex
        << gym.beat_gym_flag << std::dec << '\n'
        << "    say\n"
        << page_source(gym.guide_post_battle.pages, "        ");
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/pewter_gym.sexpr",
        .bytes = std::vector<std::uint8_t>(
            text.begin(), text.end()),
    };
}

} // namespace

bool decode_campaign_program_import(std::span<const std::uint8_t> rom,
                                    CampaignProgramImport& result, std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;

    std::vector<ToggleActor> toggle_actors;
    if (!decode_toggle_actors(rom, toggle_actors, error))
        return false;
    std::uint16_t inventory_stack_capacity = 0U;
    if (!decode_inventory_stack_capacity(
            rom, inventory_stack_capacity, error))
        return false;
    CampaignInitialState initial_state;
    if (!decode_campaign_initial_state(
            rom, initial_state, error))
        return false;
    std::vector<ImportedItemName> item_names;
    DecodedTextProgram found_item;
    DecodedTextProgram no_item_room;
    if (!decode_item_names(rom, item_names, error) ||
        !decode_loose_item_presentation(
            rom, found_item, no_item_room, error))
        return false;

    NamingProfile naming_profile;
    std::string nickname_heading;
    DecodedTextProgram nickname_question;
    if (!decode_naming_profile(rom, naming_profile, nickname_heading,
                               error) ||
        !decode_text_program(rom, 0x01U,
                             kNicknameQuestionTextOffset,
                             nickname_question) ||
        nickname_question.pages.empty()) {
        if (error.empty())
            error =
                "Pokemon nickname question could not be decoded from the pinned ROM";
        return false;
    }

    constexpr std::array<std::uint8_t, 12> default_signature{
        0xFAU, 0x47U, 0xD7U, 0xCBU, 0x47U, 0xC0U, 0xFAU, 0x61U, 0xD3U, 0xFEU, 0x01U, 0xC0U};
    constexpr std::array<std::uint8_t, 5> appeared_signature{0x21U, 0x4BU, 0xD7U, 0xCBU, 0xFEU};
    if (!has_bytes(rom, kPalletDefaultScriptOffset, default_signature) ||
        !has_bytes(rom, kPalletOakAppearedOffset, appeared_signature)) {
        error = "Pallet campaign control-flow signatures do not match the pinned ROM";
        return false;
    }

    DecodedTextProgram hey_wait;
    DecodedTextProgram unsafe;
    if (!decode_text_program(rom, 0x06U, kPalletHeyWaitTextOffset, hey_wait) ||
        !decode_text_program(rom, 0x06U, kPalletUnsafeTextOffset, unsafe) ||
        hey_wait.pages.empty() || unsafe.pages.empty()) {
        error = "Pallet campaign dialogue could not be decoded from the pinned ROM";
        return false;
    }
    std::array<DecodedTextProgram, 4> lab_choice_text;
    for (std::size_t index = 0U; index < lab_choice_text.size(); ++index) {
        if (!decode_text_program(rom, 0x07U, kOakLabChoiceTextOffsets[index],
                                 lab_choice_text[index]) ||
            lab_choice_text[index].pages.empty()) {
            error = "Oak's Lab choice dialogue could not be decoded from the pinned ROM";
            return false;
        }
    }
    std::array<DecodedTextProgram, 3> starter_prompts;
    for (std::size_t index = 0U; index < starter_prompts.size(); ++index) {
        if (!decode_text_program(rom, 0x07U,
                                 kStarterPromptTextOffsets[index],
                                 starter_prompts[index]) ||
            starter_prompts[index].pages.empty()) {
            error =
                "Oak's Lab starter prompt could not be decoded from the pinned ROM";
            return false;
        }
    }
    DecodedTextProgram starter_energetic;
    DecodedTextProgram player_received;
    DecodedTextProgram rival_takes;
    DecodedTextProgram rival_received;
    DecodedTextProgram rival_challenge;
    DecodedTextProgram rival_player_victory;
    DecodedTextProgram rival_player_loss;
    DecodedTextProgram rival_exit_text;
    if (!decode_text_program(rom, 0x07U, kStarterEnergeticTextOffset,
                             starter_energetic) ||
        !decode_text_program(rom, 0x25U,
                             kPlayerReceivedStarterTextOffset,
                             player_received) ||
        !decode_text_program(rom, 0x25U,
                             kRivalTakesStarterTextOffset, rival_takes) ||
        !decode_text_program(rom, 0x25U,
                             kRivalReceivedStarterTextOffset,
                             rival_received) ||
        !decode_text_program(rom, 0x25U, kRivalChallengeTextOffset,
                             rival_challenge) ||
        !decode_text_program(rom, 0x25U,
                             kRivalPlayerVictoryTextOffset,
                             rival_player_victory) ||
        !decode_text_program(rom, 0x25U,
                             kRivalPlayerLossTextOffset,
                             rival_player_loss) ||
        !decode_text_program(rom, 0x25U, kRivalExitTextOffset,
                             rival_exit_text) ||
        starter_energetic.pages.empty() ||
        player_received.pages.empty() || rival_takes.pages.empty() ||
        rival_received.pages.empty() || rival_challenge.pages.empty() ||
        rival_player_victory.pages.empty() ||
        rival_player_loss.pages.empty() || rival_exit_text.pages.empty()) {
        error =
            "Oak's Lab starter result dialogue could not be decoded from the pinned ROM";
        return false;
    }

    std::array<StarterChoice, 3> starter_choices;
    for (std::size_t index = 0U; index < starter_choices.size(); ++index)
        if (!decode_starter_choice(rom, kStarterChoiceCodeOffsets[index],
                                   starter_choices[index], error))
            return false;
    std::array<RivalBattleChoice, 3> rival_battles;
    if (!decode_rival_battle_choices(rom, starter_choices,
                                     rival_battles, error))
        return false;
    ParcelProgram parcel;
    if (!decode_viridian_mart_parcel(rom, parcel, error))
        return false;
    OakReturnProgram oak_return;
    if (!decode_oak_return_program(
            rom, parcel, toggle_actors, oak_return, error))
        return false;
    Route22FirstRivalProgram route_22_first_rival;
    if (!decode_route_22_first_rival(
            rom, starter_choices, toggle_actors, oak_return,
            route_22_first_rival, error))
        return false;
    OakPokeballProgram oak_pokeballs;
    if (!decode_oak_pokeball_program(
            rom, route_22_first_rival, oak_pokeballs,
            error))
        return false;
    DaisyTownMapProgram daisy_town_map;
    if (!decode_daisy_town_map_program(
            rom, toggle_actors, oak_return, daisy_town_map,
            error))
        return false;
    PalletRewardUpdates pallet_reward_updates;
    if (!decode_pallet_reward_updates(
            rom, toggle_actors, oak_pokeballs,
            daisy_town_map, pallet_reward_updates, error))
        return false;
    Route1PotionProgram route_1_potion;
    if (!decode_route_1_potion_program(
            rom, route_1_potion, error))
        return false;
    PewterGymProgram pewter_gym;
    if (!decode_pewter_gym_program(
            rom, toggle_actors, item_names,
            route_22_first_rival, pewter_gym, error))
        return false;
    PewterCityProgram pewter_city;
    if (!decode_pewter_city_program(
            rom, pewter_gym, pewter_city, error))
        return false;
    MtMoonFossilProgram mt_moon_fossils;
    if (!decode_mt_moon_fossil_program(
            rom, toggle_actors, item_names,
            mt_moon_fossils, error))
        return false;
    MtMoonMagikarpProgram mt_moon_magikarp;
    if (!decode_mt_moon_magikarp_program(
            rom, mt_moon_magikarp, error))
        return false;
    CeruleanRivalProgram cerulean_rival;
    if (!decode_cerulean_rival_program(
            rom, starter_choices, toggle_actors,
            cerulean_rival, error))
        return false;

    std::vector<PathCommand> oak_path;
    std::vector<PathCommand> player_path;
    std::vector<PathCommand> lab_oak_path;
    std::vector<PathCommand> lab_player_path;
    if (!decode_rle_path(rom, kPalletOakPathOffset, false, oak_path, error) ||
        !decode_rle_path(rom, kPalletPlayerPathOffset, true, player_path, error) ||
        !decode_direct_npc_path(rom, kOakLabEntryPathOffset, lab_oak_path, error) ||
        !decode_rle_path(rom, kOakLabPlayerPathOffset, true, lab_player_path, error))
        return false;
    std::array<std::vector<PathCommand>, 3> rival_starter_paths;
    for (std::size_t index = 0U; index < rival_starter_paths.size();
         ++index)
        if (!decode_direct_npc_path(
                rom, kRivalStarterPathOffsets[index],
                rival_starter_paths[index], error))
            return false;
    std::vector<PathCommand> rival_exit_path;
    if (!decode_direct_npc_path(rom, kRivalExitPathOffset,
                                rival_exit_path, error))
        return false;

    // The simulated joypad buffer is consumed from its final index toward zero.
    // Reverse its decoded command stream; MoveSprite consumes Oak's stream forward.
    std::reverse(player_path.begin(), player_path.end());
    std::reverse(lab_player_path.begin(), lab_player_path.end());
    if (player_path.size() < 2U || player_path[player_path.size() - 1U] != PathCommand::up ||
        player_path[player_path.size() - 2U] != PathCommand::up) {
        error = "Pallet player path no longer ends in the verified lab transition";
        return false;
    }
    // Native warps materialize directly on their destination cell. The second
    // original UP is consumed by the Game Boy transition, so the normalized
    // native path stops after crossing the warp.
    player_path.pop_back();

    std::vector<Instruction> instructions;
    instructions.push_back(operation(Opcode::lock_input));
    instructions.push_back(operation(Opcode::set_flag, 0U, 0U, kOakAppearedFlag));
    instructions.push_back(dialogue(hey_wait.pages));
    instructions.push_back(operation(Opcode::show_actor, 1U, 0U, 0U));
    instructions.push_back(operation(Opcode::face_actor, 1U, 1U, 0U));
    instructions.push_back(operation(Opcode::move_actor_to_player, 1U, 1U, 0U));
    instructions.push_back(operation(Opcode::face_player));
    instructions.push_back(dialogue(unsafe.pages));
    instructions.push_back(operation(Opcode::align_pair_x, 1U, 10U, 0U));
    Instruction walk = operation(Opcode::parallel_path, 1U, 1U, 0U);
    walk.actor_path = oak_path;
    walk.player_path = player_path;
    instructions.push_back(std::move(walk));
    instructions.push_back(operation(Opcode::show_actor, 8U, 0U, 40U));
    Instruction oak_enters = operation(Opcode::parallel_path, 8U, 1U, 40U);
    oak_enters.actor_path = lab_oak_path;
    oak_enters.player_path = std::vector<PathCommand>(lab_oak_path.size(), PathCommand::wait);
    instructions.push_back(std::move(oak_enters));
    instructions.push_back(operation(Opcode::show_actor, 5U, 0U, 40U));
    instructions.push_back(operation(Opcode::face_actor, 5U, 0U, 40U));
    instructions.push_back(operation(Opcode::face_actor, 1U, 1U, 40U));
    Instruction player_enters = operation(Opcode::parallel_path, 5U, 0U, 40U);
    player_enters.actor_path = std::vector<PathCommand>(lab_player_path.size(), PathCommand::wait);
    player_enters.player_path = lab_player_path;
    instructions.push_back(std::move(player_enters));
    instructions.push_back(operation(Opcode::set_flag, 0U, 0U, kFollowedOakFlag));
    instructions.push_back(operation(Opcode::set_flag, 0U, 0U, kFollowedOakSecondFlag));
    for (const DecodedTextProgram& text : lab_choice_text)
        instructions.push_back(dialogue(text.pages));
    instructions.push_back(operation(Opcode::set_flag, 0U, 0U, kOakAskedToChooseFlag));
    instructions.push_back(operation(Opcode::unlock_input));
    instructions.push_back(operation(Opcode::end));

    Program opening;
    opening.key = "pallet_oak_interception";
    opening.trigger_map = 0U;
    opening.trigger_y = 1U;
    opening.absent_flag = kFollowedOakFlag;
    for (const ToggleActor& actor : toggle_actors)
        if (!actor.initially_visible)
            opening.initially_hidden.push_back(
                {actor.map_id, actor.actor_index});
    opening.instructions = std::move(instructions);

    constexpr std::array<std::string_view, 3> starter_keys{
        "oaks_lab_choose_charmander",
        "oaks_lab_choose_squirtle",
        "oaks_lab_choose_bulbasaur",
    };
    constexpr std::array<std::string_view, 3> rival_battle_keys{
        "oaks_lab_first_rival_after_charmander",
        "oaks_lab_first_rival_after_squirtle",
        "oaks_lab_first_rival_after_bulbasaur",
    };
    constexpr std::array<std::string_view, 3>
        route_22_rival_keys{
            "route_22_first_rival_after_charmander",
            "route_22_first_rival_after_squirtle",
            "route_22_first_rival_after_bulbasaur",
        };
    std::vector<Program> programs;
    programs.reserve(3U + starter_choices.size() +
                     rival_battles.size() +
                     route_22_first_rival.battles.size());
    programs.push_back(std::move(opening));
    for (std::size_t index = 0U; index < starter_choices.size(); ++index) {
        const StarterChoice& choice = starter_choices[index];
        Program starter;
        starter.key = starter_keys[index];
        starter.trigger_kind = TriggerKind::actor_activation;
        starter.trigger_map = 40U;
        starter.trigger_x = choice.selected_ball;
        starter.required_flag = kOakAskedToChooseFlag;
        starter.absent_flag = kGotStarterFlag;

        starter.instructions.push_back(operation(Opcode::lock_input));
        starter.instructions.push_back(
            ask_yes_no(starter_prompts[index].pages,
                       choice.player_species));
        starter.instructions.push_back(
            operation(Opcode::end_if_choice_no));
        starter.instructions.push_back(operation(
            Opcode::hide_actor, choice.selected_ball, 0U, 40U));
        starter.instructions.push_back(species_dialogue(
            starter_energetic.pages, choice.player_species));
        starter.instructions.push_back(species_dialogue(
            player_received.pages, choice.player_species));
        starter.instructions.push_back(operation(
            Opcode::give_pokemon, 5U, 0U, choice.player_species));
        starter.instructions.push_back(ask_yes_no(
            nickname_question.pages, choice.player_species));
        starter.instructions.push_back(operation(
            Opcode::nickname_last_party_member_if_yes));
        starter.instructions.push_back(operation(
            Opcode::set_variable, kPlayerStarterVariable, 0U,
            choice.player_species));
        starter.instructions.push_back(operation(
            Opcode::set_variable, kRivalStarterVariable, 0U,
            choice.rival_species));

        Instruction rival_walk =
            operation(Opcode::parallel_path, 1U, 0U, 40U);
        rival_walk.actor_path = rival_starter_paths[index];
        rival_walk.player_path = std::vector<PathCommand>(
            rival_walk.actor_path.size(), PathCommand::wait);
        starter.instructions.push_back(std::move(rival_walk));
        starter.instructions.push_back(
            operation(Opcode::face_actor, 1U, 1U, 40U));
        starter.instructions.push_back(dialogue(rival_takes.pages));
        starter.instructions.push_back(operation(
            Opcode::hide_actor, choice.rival_ball, 0U, 40U));
        starter.instructions.push_back(species_dialogue(
            rival_received.pages, choice.rival_species));
        starter.instructions.push_back(
            operation(Opcode::set_flag, 0U, 0U, kGotStarterFlag));
        starter.instructions.push_back(operation(Opcode::unlock_input));
        starter.instructions.push_back(operation(Opcode::end));
        programs.push_back(std::move(starter));
    }

    for (std::size_t index = 0U; index < rival_battles.size();
         ++index) {
        const RivalBattleChoice& battle = rival_battles[index];
        Program rival;
        rival.key = rival_battle_keys[index];
        rival.trigger_map = 40U;
        rival.trigger_y = 6U;
        rival.required_flag = kGotStarterFlag;
        rival.absent_flag = kBattledLabRivalFlag;
        rival.required_variable = kRivalStarterVariable;
        rival.required_variable_value = battle.rival_species;

        rival.instructions.push_back(operation(Opcode::lock_input));
        rival.instructions.push_back(
            operation(Opcode::face_actor, 1U, 0U, 40U));
        rival.instructions.push_back(
            operation(Opcode::face_player, 1U));
        rival.instructions.push_back(dialogue(rival_challenge.pages));
        rival.instructions.push_back(operation(
            Opcode::move_actor_to_player, 1U, 0xFFU, 40U));
        rival.instructions.push_back(operation(
            Opcode::start_trainer_battle, battle.trainer_class, 0U,
            battle.trainer_party));

        Instruction victory =
            operation(Opcode::say_if_player_won);
        victory.pages = rival_player_victory.pages;
        rival.instructions.push_back(std::move(victory));
        Instruction loss = operation(Opcode::say_if_player_lost);
        loss.pages = rival_player_loss.pages;
        rival.instructions.push_back(std::move(loss));
        rival.instructions.push_back(
            operation(Opcode::face_actor, 1U, 0U, 40U));
        rival.instructions.push_back(operation(Opcode::heal_party));
        rival.instructions.push_back(operation(
            Opcode::set_flag, 0U, 0U, kBattledLabRivalFlag));
        rival.instructions.push_back(
            operation(Opcode::wait_ticks, 0U, 0U, 20U));
        rival.instructions.push_back(dialogue(rival_exit_text.pages));

        Instruction exit_path = operation(
            Opcode::actor_path_by_player_x, 1U, 4U, 40U);
        exit_path.actor_path = rival_exit_path;
        exit_path.actor_path.push_back(PathCommand::right);
        exit_path.player_path = rival_exit_path;
        exit_path.player_path.push_back(PathCommand::left);
        rival.instructions.push_back(std::move(exit_path));
        rival.instructions.push_back(
            operation(Opcode::hide_actor, 1U, 0U, 40U));
        rival.instructions.push_back(operation(Opcode::unlock_input));
        rival.instructions.push_back(operation(Opcode::end));
        programs.push_back(std::move(rival));
    }

    Program parcel_program;
    parcel_program.key = "viridian_mart_oaks_parcel";
    parcel_program.trigger_kind = TriggerKind::map_entry;
    parcel_program.trigger_map = kViridianMartMapId;
    parcel_program.absent_flag = parcel.got_oaks_parcel_flag;
    parcel_program.instructions.push_back(
        operation(Opcode::lock_input));
    parcel_program.instructions.push_back(
        dialogue(parcel.came_from_pallet.pages));
    Instruction parcel_walk = operation(Opcode::player_path);
    parcel_walk.player_path = parcel.player_path;
    parcel_program.instructions.push_back(std::move(parcel_walk));
    parcel_program.instructions.push_back(
        operation(Opcode::wait_ticks, 0U, 0U, 3U));
    parcel_program.instructions.push_back(
        dialogue(parcel.parcel_quest.pages));
    parcel_program.instructions.push_back(operation(
        Opcode::give_item, parcel.quantity, 0U, parcel.item_id));
    parcel_program.instructions.push_back(operation(
        Opcode::set_flag, 0U, 0U, parcel.got_oaks_parcel_flag));
    parcel_program.instructions.push_back(
        operation(Opcode::unlock_input));
    parcel_program.instructions.push_back(operation(Opcode::end));
    programs.push_back(std::move(parcel_program));

    Program oak_request;
    oak_request.key =
        "oaks_lab_deliver_parcel_and_get_pokedex";
    oak_request.trigger_kind = TriggerKind::actor_activation;
    oak_request.trigger_map = 40U;
    oak_request.trigger_x = 5U;
    oak_request.required_flag = kBattledLabRivalFlag;
    oak_request.absent_flag = oak_return.oak_got_parcel_flag;
    oak_request.required_item_id = oak_return.parcel_item_id;
    oak_request.required_item_quantity =
        oak_return.parcel_quantity;
    oak_request.instructions.push_back(
        operation(Opcode::lock_input));
    oak_request.instructions.push_back(
        dialogue(oak_return.deliver_parcel.pages));
    oak_request.instructions.push_back(operation(
        Opcode::take_item, oak_return.parcel_quantity, 0U,
        oak_return.parcel_item_id));
    oak_request.instructions.push_back(
        dialogue(oak_return.rival_gramps.pages));

    const auto append_rival_path_branches =
        [&](PathCommand direction, bool place, bool hide) {
            const std::size_t first_condition =
                oak_request.instructions.size();
            oak_request.instructions.push_back(operation(
                Opcode::jump_if_player_y,
                oak_return.player_y[0]));
            const std::size_t second_condition =
                oak_request.instructions.size();
            oak_request.instructions.push_back(operation(
                Opcode::jump_if_player_y,
                oak_return.player_y[1]));

            std::array<std::size_t, 2> branch_targets{};
            std::vector<std::size_t> exits;
            const auto append_branch =
                [&](std::size_t branch, bool final_branch) {
                    if (branch == 0U)
                        branch_targets[0] =
                            oak_request.instructions.size();
                    else if (branch == 1U)
                        branch_targets[1] =
                            oak_request.instructions.size();
                    if (place) {
                        oak_request.instructions.push_back(operation(
                            Opcode::place_actor, 1U, 40U,
                            packed_position(
                                oak_return.spawn_x,
                                oak_return.spawn_y[branch])));
                        oak_request.instructions.push_back(operation(
                            Opcode::show_actor, 1U, 0U, 40U));
                    }
                    Instruction path = operation(
                        Opcode::actor_path, 1U, 40U,
                        hide ? 1U : 0U);
                    path.actor_path.assign(
                        oak_return.movement_steps[branch],
                        direction);
                    oak_request.instructions.push_back(
                        std::move(path));
                    if (!final_branch) {
                        exits.push_back(
                            oak_request.instructions.size());
                        oak_request.instructions.push_back(
                            operation(Opcode::jump));
                    }
                };

            // The source's third branch covers either side of Oak.
            append_branch(2U, false);
            append_branch(0U, false);
            append_branch(1U, true);
            const std::size_t join =
                oak_request.instructions.size();
            oak_request.instructions[first_condition].value =
                static_cast<std::uint32_t>(branch_targets[0]);
            oak_request.instructions[second_condition].value =
                static_cast<std::uint32_t>(branch_targets[1]);
            for (const std::size_t exit : exits)
                oak_request.instructions[exit].value =
                    static_cast<std::uint32_t>(join);
        };

    append_rival_path_branches(
        PathCommand::up, true, false);
    oak_request.instructions.push_back(
        operation(Opcode::face_actor, 1U, 1U, 40U));
    oak_request.instructions.push_back(
        operation(Opcode::face_actor, 5U, 0U, 40U));
    oak_request.instructions.push_back(
        dialogue(oak_return.rival_what_did_you_call.pages));
    oak_request.instructions.push_back(
        operation(Opcode::wait_ticks, 0U, 0U, 1U));
    oak_request.instructions.push_back(
        dialogue(oak_return.oak_request.pages));
    oak_request.instructions.push_back(
        operation(Opcode::wait_ticks, 0U, 0U, 1U));
    oak_request.instructions.push_back(
        dialogue(oak_return.pokedex_invention.pages));
    oak_request.instructions.push_back(
        operation(Opcode::wait_ticks, 0U, 0U, 1U));
    oak_request.instructions.push_back(
        dialogue(oak_return.got_pokedex.pages));
    oak_request.instructions.push_back(
        operation(Opcode::wait_ticks, 0U, 0U, 3U));
    oak_request.instructions.push_back(operation(
        Opcode::hide_actor, oak_return.pokedex_1.actor_index,
        0U, oak_return.pokedex_1.map_id));
    oak_request.instructions.push_back(operation(
        Opcode::hide_actor, oak_return.pokedex_2.actor_index,
        0U, oak_return.pokedex_2.map_id));
    oak_request.instructions.push_back(
        dialogue(oak_return.oak_dream.pages));
    oak_request.instructions.push_back(
        operation(Opcode::face_actor, 1U, 3U, 40U));
    oak_request.instructions.push_back(
        operation(Opcode::wait_ticks, 0U, 0U, 3U));
    oak_request.instructions.push_back(
        dialogue(oak_return.rival_leave.pages));
    oak_request.instructions.push_back(operation(
        Opcode::set_flag, 0U, 0U,
        oak_return.got_pokedex_flag));
    oak_request.instructions.push_back(operation(
        Opcode::set_flag, 0U, 0U,
        oak_return.oak_got_parcel_flag));
    oak_request.instructions.push_back(operation(
        Opcode::hide_actor,
        oak_return.lying_old_man.actor_index, 0U,
        oak_return.lying_old_man.map_id));
    oak_request.instructions.push_back(operation(
        Opcode::show_actor,
        oak_return.standing_old_man.actor_index, 0U,
        oak_return.standing_old_man.map_id));
    append_rival_path_branches(
        PathCommand::down, false, true);
    oak_request.instructions.push_back(operation(
        Opcode::set_flag, 0U, 0U,
        oak_return.first_route_22_rival_flag));
    oak_request.instructions.push_back(operation(
        Opcode::clear_flag, 0U, 0U,
        oak_return.second_route_22_rival_flag));
    oak_request.instructions.push_back(operation(
        Opcode::set_flag, 0U, 0U,
        oak_return.route_22_rival_wants_battle_flag));
    oak_request.instructions.push_back(operation(
        Opcode::show_actor,
        oak_return.route_22_rival.actor_index, 0U,
        oak_return.route_22_rival.map_id));
    oak_request.instructions.push_back(
        operation(Opcode::unlock_input));
    oak_request.instructions.push_back(operation(Opcode::end));
    programs.push_back(std::move(oak_request));

    for (std::size_t index = 0U;
         index < route_22_first_rival.battles.size();
         ++index) {
        const RivalBattleChoice& battle =
            route_22_first_rival.battles[index];
        Program rival;
        rival.key = route_22_rival_keys[index];
        rival.trigger_kind = TriggerKind::player_rectangle;
        rival.trigger_map = route_22_first_rival.map_id;
        rival.trigger_x = route_22_first_rival.trigger_x;
        rival.trigger_y = route_22_first_rival.trigger_y;
        rival.trigger_width =
            route_22_first_rival.trigger_width;
        rival.trigger_height =
            route_22_first_rival.trigger_height;
        rival.required_flag =
            route_22_first_rival.wants_battle_flag;
        rival.absent_flag =
            route_22_first_rival.beat_rival_flag;
        rival.required_variable = kRivalStarterVariable;
        rival.required_variable_value =
            battle.rival_species;

        rival.instructions.push_back(
            operation(Opcode::lock_input));
        Instruction approach = operation(
            Opcode::actor_path_by_player_y,
            route_22_first_rival.actor_index,
            static_cast<std::uint8_t>(
                route_22_first_rival.trigger_y +
                route_22_first_rival.trigger_height - 1U),
            route_22_first_rival.map_id);
        approach.actor_path =
            route_22_first_rival.approach_lower;
        approach.player_path =
            route_22_first_rival.approach_upper;
        rival.instructions.push_back(std::move(approach));

        const std::size_t lower_condition =
            rival.instructions.size();
        rival.instructions.push_back(operation(
            Opcode::jump_if_player_y,
            static_cast<std::uint8_t>(
                route_22_first_rival.trigger_y +
                route_22_first_rival.trigger_height - 1U)));
        rival.instructions.push_back(operation(
            Opcode::face_actor,
            route_22_first_rival.actor_index, 1U,
            route_22_first_rival.map_id));
        rival.instructions.push_back(
            operation(Opcode::face_player, 0U));
        const std::size_t upper_exit =
            rival.instructions.size();
        rival.instructions.push_back(
            operation(Opcode::jump));
        const std::size_t lower_branch =
            rival.instructions.size();
        rival.instructions.push_back(operation(
            Opcode::face_actor,
            route_22_first_rival.actor_index, 3U,
            route_22_first_rival.map_id));
        rival.instructions.push_back(
            operation(Opcode::face_player, 2U));
        const std::size_t face_join =
            rival.instructions.size();
        rival.instructions[lower_condition].value =
            static_cast<std::uint32_t>(lower_branch);
        rival.instructions[upper_exit].value =
            static_cast<std::uint32_t>(face_join);

        rival.instructions.push_back(dialogue(
            route_22_first_rival.before_battle.pages));
        rival.instructions.push_back(operation(
            Opcode::start_trainer_battle,
            battle.trainer_class, 0U,
            battle.trainer_party));
        Instruction defeated =
            operation(Opcode::say_if_player_won);
        defeated.pages =
            route_22_first_rival.defeated.pages;
        rival.instructions.push_back(std::move(defeated));
        Instruction victory =
            operation(Opcode::say_if_player_lost);
        victory.pages =
            route_22_first_rival.victory.pages;
        rival.instructions.push_back(std::move(victory));
        rival.instructions.push_back(
            operation(Opcode::end_if_player_lost));
        rival.instructions.push_back(operation(
            Opcode::set_flag, 0U, 0U,
            route_22_first_rival.beat_rival_flag));
        rival.instructions.push_back(dialogue(
            route_22_first_rival.after_battle.pages));

        Instruction exit = operation(
            Opcode::actor_path_by_player_y,
            route_22_first_rival.actor_index,
            static_cast<std::uint8_t>(
                route_22_first_rival.trigger_y +
                route_22_first_rival.trigger_height - 1U),
            route_22_first_rival.map_id);
        exit.actor_path =
            route_22_first_rival.exit_lower;
        exit.player_path =
            route_22_first_rival.exit_upper;
        rival.instructions.push_back(std::move(exit));
        rival.instructions.push_back(operation(
            Opcode::hide_actor,
            route_22_first_rival.actor_index, 0U,
            route_22_first_rival.map_id));
        rival.instructions.push_back(operation(
            Opcode::clear_flag, 0U, 0U,
            route_22_first_rival.first_rival_flag));
        rival.instructions.push_back(operation(
            Opcode::clear_flag, 0U, 0U,
            route_22_first_rival.wants_battle_flag));
        rival.instructions.push_back(
            operation(Opcode::unlock_input));
        rival.instructions.push_back(operation(Opcode::end));
        programs.push_back(std::move(rival));
    }

    Program oak_balls;
    oak_balls.key = "oaks_lab_give_pokeballs";
    oak_balls.trigger_kind = TriggerKind::actor_activation;
    oak_balls.trigger_map = 40U;
    oak_balls.trigger_x = 5U;
    oak_balls.required_flag =
        oak_pokeballs.beat_route_22_rival_flag;
    oak_balls.absent_flag =
        oak_pokeballs.got_pokeballs_flag;
    oak_balls.instructions.push_back(
        operation(Opcode::lock_input));
    oak_balls.instructions.push_back(operation(
        Opcode::set_flag, 0U, 0U,
        oak_pokeballs.got_pokeballs_flag));
    oak_balls.instructions.push_back(operation(
        Opcode::try_give_item, oak_pokeballs.quantity,
        0U, oak_pokeballs.item_id));
    oak_balls.instructions.push_back(
        dialogue(oak_pokeballs.give_pokeballs.pages));
    oak_balls.instructions.push_back(
        operation(Opcode::unlock_input));
    oak_balls.instructions.push_back(operation(Opcode::end));
    programs.push_back(std::move(oak_balls));

    Program oak_after_balls;
    oak_after_balls.key = "oaks_lab_after_pokeballs";
    oak_after_balls.trigger_kind =
        TriggerKind::actor_activation;
    oak_after_balls.trigger_map = 40U;
    oak_after_balls.trigger_x = 5U;
    oak_after_balls.required_flag =
        oak_pokeballs.got_pokeballs_flag;
    oak_after_balls.instructions.push_back(
        operation(Opcode::lock_input));
    oak_after_balls.instructions.push_back(
        dialogue(oak_pokeballs.come_see_me.pages));
    oak_after_balls.instructions.push_back(
        operation(Opcode::unlock_input));
    oak_after_balls.instructions.push_back(operation(Opcode::end));
    programs.push_back(std::move(oak_after_balls));

    Program daisy_before;
    daisy_before.key = "blues_house_daisy_before_pokedex";
    daisy_before.trigger_kind =
        TriggerKind::actor_activation;
    daisy_before.trigger_map = 39U;
    daisy_before.trigger_x = 1U;
    daisy_before.absent_flag =
        daisy_town_map.got_pokedex_flag;
    daisy_before.instructions.push_back(
        operation(Opcode::lock_input));
    daisy_before.instructions.push_back(
        dialogue(daisy_town_map.rival_at_lab.pages));
    daisy_before.instructions.push_back(
        operation(Opcode::unlock_input));
    daisy_before.instructions.push_back(operation(Opcode::end));
    programs.push_back(std::move(daisy_before));

    Program daisy_gift;
    daisy_gift.key = "blues_house_daisy_give_town_map";
    daisy_gift.trigger_kind =
        TriggerKind::actor_activation;
    daisy_gift.trigger_map = 39U;
    daisy_gift.trigger_x = 1U;
    daisy_gift.required_flag =
        daisy_town_map.got_pokedex_flag;
    daisy_gift.absent_flag =
        daisy_town_map.got_town_map_flag;
    daisy_gift.instructions.push_back(
        operation(Opcode::lock_input));
    daisy_gift.instructions.push_back(
        dialogue(daisy_town_map.offer_map.pages));
    daisy_gift.instructions.push_back(operation(
        Opcode::try_give_item, daisy_town_map.quantity,
        0U, daisy_town_map.item_id));
    const std::size_t bag_full_jump =
        daisy_gift.instructions.size();
    daisy_gift.instructions.push_back(
        operation(Opcode::jump_if_item_grant_failed));
    daisy_gift.instructions.push_back(operation(
        Opcode::hide_actor,
        daisy_town_map.town_map_actor.actor_index, 0U,
        daisy_town_map.town_map_actor.map_id));
    daisy_gift.instructions.push_back(
        dialogue(daisy_town_map.got_map.pages));
    daisy_gift.instructions.push_back(operation(
        Opcode::set_flag, 0U, 0U,
        daisy_town_map.got_town_map_flag));
    daisy_gift.instructions.push_back(
        operation(Opcode::unlock_input));
    daisy_gift.instructions.push_back(operation(Opcode::end));
    const std::size_t bag_full_branch =
        daisy_gift.instructions.size();
    daisy_gift.instructions[bag_full_jump].value =
        static_cast<std::uint32_t>(bag_full_branch);
    daisy_gift.instructions.push_back(
        dialogue(daisy_town_map.bag_full.pages));
    daisy_gift.instructions.push_back(
        operation(Opcode::unlock_input));
    daisy_gift.instructions.push_back(operation(Opcode::end));
    programs.push_back(std::move(daisy_gift));

    Program daisy_after;
    daisy_after.key =
        "blues_house_daisy_after_town_map";
    daisy_after.trigger_kind =
        TriggerKind::actor_activation;
    daisy_after.trigger_map = 39U;
    daisy_after.trigger_x = 1U;
    daisy_after.required_flag =
        daisy_town_map.got_town_map_flag;
    daisy_after.instructions.push_back(
        operation(Opcode::lock_input));
    daisy_after.instructions.push_back(
        dialogue(daisy_town_map.use_map.pages));
    daisy_after.instructions.push_back(
        operation(Opcode::unlock_input));
    daisy_after.instructions.push_back(operation(Opcode::end));
    programs.push_back(std::move(daisy_after));

    const auto map_flag_update =
        [&](std::string key, std::uint8_t map_id,
            std::uint32_t required_flag,
            std::uint32_t absent_flag,
            std::uint32_t set_flag) {
            Program program;
            program.key = std::move(key);
            program.trigger_kind = TriggerKind::map_presence;
            program.trigger_map = map_id;
            program.required_flag = required_flag;
            program.absent_flag = absent_flag;
            program.instructions.push_back(operation(
                Opcode::set_flag, 0U, 0U, set_flag));
            program.instructions.push_back(
                operation(Opcode::end));
            return program;
        };
    programs.push_back(map_flag_update(
        "blues_house_record_entry", 39U, 0xFFFFFFFFU,
        pallet_reward_updates.entered_blues_house_flag,
        pallet_reward_updates.entered_blues_house_flag));
    programs.push_back(map_flag_update(
        "pallet_after_pokeballs", 0U,
        pallet_reward_updates.got_pokeballs_flag,
        pallet_reward_updates.pallet_after_pokeballs_flag,
        pallet_reward_updates.pallet_after_pokeballs_flag));

    Program daisy_walks;
    daisy_walks.key = "pallet_daisy_starts_walking";
    daisy_walks.trigger_kind = TriggerKind::map_presence;
    daisy_walks.trigger_map = 0U;
    daisy_walks.required_flag =
        pallet_reward_updates.got_town_map_flag;
    daisy_walks.absent_flag =
        pallet_reward_updates.daisy_walking_flag;
    daisy_walks.instructions.push_back(operation(
        Opcode::set_flag, 0U, 0U,
        pallet_reward_updates.daisy_walking_flag));
    daisy_walks.instructions.push_back(operation(
        Opcode::hide_actor,
        pallet_reward_updates.daisy_sitting.actor_index,
        0U, pallet_reward_updates.daisy_sitting.map_id));
    daisy_walks.instructions.push_back(operation(
        Opcode::show_actor,
        pallet_reward_updates.daisy_walking.actor_index,
        0U, pallet_reward_updates.daisy_walking.map_id));
    daisy_walks.instructions.push_back(operation(Opcode::end));
    programs.push_back(std::move(daisy_walks));

    programs.push_back(map_flag_update(
        "pallet_after_pokeballs_2", 0U,
        pallet_reward_updates.got_pokeballs_flag,
        pallet_reward_updates.pallet_after_pokeballs_2_flag,
        pallet_reward_updates.pallet_after_pokeballs_2_flag));

    Program potion_sample;
    potion_sample.key = "route_1_potion_sample";
    potion_sample.trigger_kind =
        TriggerKind::actor_activation;
    potion_sample.trigger_map = 12U;
    potion_sample.trigger_x = 1U;
    potion_sample.absent_flag =
        route_1_potion.got_potion_sample_flag;
    potion_sample.instructions.push_back(
        operation(Opcode::lock_input));
    potion_sample.instructions.push_back(operation(
        Opcode::set_flag, 0U, 0U,
        route_1_potion.got_potion_sample_flag));
    potion_sample.instructions.push_back(
        dialogue(route_1_potion.sample.pages));
    potion_sample.instructions.push_back(operation(
        Opcode::try_give_item, route_1_potion.quantity,
        0U, route_1_potion.item_id));
    const std::size_t potion_full_jump =
        potion_sample.instructions.size();
    potion_sample.instructions.push_back(
        operation(Opcode::jump_if_item_grant_failed));
    potion_sample.instructions.push_back(
        dialogue(route_1_potion.got_item.pages));
    potion_sample.instructions.push_back(
        operation(Opcode::unlock_input));
    potion_sample.instructions.push_back(
        operation(Opcode::end));
    potion_sample.instructions[potion_full_jump].value =
        static_cast<std::uint32_t>(
            potion_sample.instructions.size());
    potion_sample.instructions.push_back(
        dialogue(route_1_potion.no_room.pages));
    potion_sample.instructions.push_back(
        operation(Opcode::unlock_input));
    potion_sample.instructions.push_back(
        operation(Opcode::end));
    programs.push_back(std::move(potion_sample));

    Program potion_repeat;
    potion_repeat.key = "route_1_after_potion_sample";
    potion_repeat.trigger_kind =
        TriggerKind::actor_activation;
    potion_repeat.trigger_map = 12U;
    potion_repeat.trigger_x = 1U;
    potion_repeat.required_flag =
        route_1_potion.got_potion_sample_flag;
    potion_repeat.instructions.push_back(
        operation(Opcode::lock_input));
    potion_repeat.instructions.push_back(
        dialogue(route_1_potion.repeat.pages));
    potion_repeat.instructions.push_back(
        operation(Opcode::unlock_input));
    potion_repeat.instructions.push_back(
        operation(Opcode::end));
    programs.push_back(std::move(potion_repeat));

    const auto append_pewter_reward =
        [&](std::vector<Instruction>& reward_instructions) {
            reward_instructions.push_back(
                dialogue(pewter_gym.wait_take_this.pages));
            reward_instructions.push_back(operation(
                Opcode::set_flag, 0U, 0U,
                pewter_gym.beat_brock_flag));
            reward_instructions.push_back(operation(
                Opcode::try_give_item,
                pewter_gym.tm_quantity, 0U,
                pewter_gym.tm_item_id));
            const std::size_t pewter_bag_full_jump =
                reward_instructions.size();
            reward_instructions.push_back(operation(
                Opcode::jump_if_item_grant_failed));
            reward_instructions.push_back(
                dialogue(pewter_gym.received_tm.pages));
            reward_instructions.push_back(operation(
                Opcode::set_flag, 0U, 0U,
                pewter_gym.got_tm_flag));
            const std::size_t victory_jump =
                reward_instructions.size();
            reward_instructions.push_back(
                operation(Opcode::jump));
            reward_instructions[pewter_bag_full_jump].value =
                static_cast<std::uint32_t>(
                    reward_instructions.size());
            reward_instructions.push_back(
                dialogue(pewter_gym.no_room.pages));
            reward_instructions[victory_jump].value =
                static_cast<std::uint32_t>(
                    reward_instructions.size());
            reward_instructions.push_back(operation(
                Opcode::set_flag, 0U, 0U,
                pewter_gym.obtained_badge_flag));
            reward_instructions.push_back(operation(
                Opcode::set_flag, 0U, 0U,
                pewter_gym.beat_gym_flag));
            reward_instructions.push_back(operation(
                Opcode::hide_actor,
                pewter_gym.gym_guide.actor_index, 0U,
                pewter_gym.gym_guide.map_id));
            reward_instructions.push_back(operation(
                Opcode::hide_actor,
                pewter_gym.route_rival.actor_index, 0U,
                pewter_gym.route_rival.map_id));
            reward_instructions.push_back(operation(
                Opcode::clear_flag, 0U, 0U,
                pewter_gym.first_route_rival_flag));
            reward_instructions.push_back(operation(
                Opcode::clear_flag, 0U, 0U,
                pewter_gym.route_rival_wants_battle_flag));
            reward_instructions.push_back(operation(
                Opcode::set_flag, 0U, 0U,
                pewter_gym.gym_trainer_defeated_flag));
            reward_instructions.push_back(
                operation(Opcode::unlock_input));
            reward_instructions.push_back(
                operation(Opcode::end));
        };

    Program brock_battle;
    brock_battle.key = "pewter_gym_brock_battle";
    brock_battle.trigger_kind =
        TriggerKind::actor_activation;
    brock_battle.trigger_map = pewter_gym.map_id;
    brock_battle.trigger_x =
        pewter_gym.brock_actor_index;
    brock_battle.absent_flag =
        pewter_gym.beat_brock_flag;
    brock_battle.instructions.push_back(
        operation(Opcode::lock_input));
    brock_battle.instructions.push_back(
        dialogue(pewter_gym.pre_battle.pages));
    brock_battle.instructions.push_back(operation(
        Opcode::start_trainer_battle,
        pewter_gym.trainer_class, 0U,
        pewter_gym.trainer_party));
    Instruction badge = operation(
        Opcode::say_if_player_won);
    badge.pages = pewter_gym.badge.pages;
    brock_battle.instructions.push_back(std::move(badge));
    brock_battle.instructions.push_back(
        operation(Opcode::end_if_player_lost));
    append_pewter_reward(brock_battle.instructions);
    programs.push_back(std::move(brock_battle));

    Program brock_tm_retry;
    brock_tm_retry.key = "pewter_gym_brock_tm_retry";
    brock_tm_retry.trigger_kind =
        TriggerKind::actor_activation;
    brock_tm_retry.trigger_map = pewter_gym.map_id;
    brock_tm_retry.trigger_x =
        pewter_gym.brock_actor_index;
    brock_tm_retry.required_flag =
        pewter_gym.beat_brock_flag;
    brock_tm_retry.absent_flag =
        pewter_gym.got_tm_flag;
    brock_tm_retry.instructions.push_back(
        operation(Opcode::lock_input));
    append_pewter_reward(brock_tm_retry.instructions);
    programs.push_back(std::move(brock_tm_retry));

    Program brock_advice;
    brock_advice.key = "pewter_gym_brock_advice";
    brock_advice.trigger_kind =
        TriggerKind::actor_activation;
    brock_advice.trigger_map = pewter_gym.map_id;
    brock_advice.trigger_x =
        pewter_gym.brock_actor_index;
    brock_advice.required_flag =
        pewter_gym.got_tm_flag;
    brock_advice.instructions.push_back(
        operation(Opcode::lock_input));
    brock_advice.instructions.push_back(
        dialogue(pewter_gym.post_battle_advice.pages));
    brock_advice.instructions.push_back(
        operation(Opcode::unlock_input));
    brock_advice.instructions.push_back(
        operation(Opcode::end));
    programs.push_back(std::move(brock_advice));

    Program guide_before;
    guide_before.key = "pewter_gym_guide_before_brock";
    guide_before.trigger_kind =
        TriggerKind::actor_activation;
    guide_before.trigger_map = pewter_gym.map_id;
    guide_before.trigger_x =
        pewter_gym.guide_actor_index;
    guide_before.absent_flag =
        pewter_gym.beat_gym_flag;
    guide_before.instructions.push_back(
        operation(Opcode::lock_input));
    Instruction guide_question =
        operation(Opcode::ask_yes_no);
    guide_question.pages =
        pewter_gym.guide_pre_advice.pages;
    guide_before.instructions.push_back(
        std::move(guide_question));
    const std::size_t guide_no_jump =
        guide_before.instructions.size();
    guide_before.instructions.push_back(
        operation(Opcode::jump_if_choice_no));
    guide_before.instructions.push_back(
        dialogue(pewter_gym.guide_begin_advice.pages));
    const std::size_t guide_advice_jump =
        guide_before.instructions.size();
    guide_before.instructions.push_back(
        operation(Opcode::jump));
    guide_before.instructions[guide_no_jump].value =
        static_cast<std::uint32_t>(
            guide_before.instructions.size());
    guide_before.instructions.push_back(
        dialogue(pewter_gym.guide_free_service.pages));
    guide_before.instructions[guide_advice_jump].value =
        static_cast<std::uint32_t>(
            guide_before.instructions.size());
    guide_before.instructions.push_back(
        dialogue(pewter_gym.guide_advice.pages));
    guide_before.instructions.push_back(
        operation(Opcode::unlock_input));
    guide_before.instructions.push_back(
        operation(Opcode::end));
    programs.push_back(std::move(guide_before));

    Program guide_after;
    guide_after.key = "pewter_gym_guide_after_brock";
    guide_after.trigger_kind =
        TriggerKind::actor_activation;
    guide_after.trigger_map = pewter_gym.map_id;
    guide_after.trigger_x =
        pewter_gym.guide_actor_index;
    guide_after.required_flag =
        pewter_gym.beat_gym_flag;
    guide_after.instructions.push_back(
        operation(Opcode::lock_input));
    guide_after.instructions.push_back(
        dialogue(pewter_gym.guide_post_battle.pages));
    guide_after.instructions.push_back(
        operation(Opcode::unlock_input));
    guide_after.instructions.push_back(
        operation(Opcode::end));
    programs.push_back(std::move(guide_after));

    Program museum_guide;
    museum_guide.key = "pewter_city_museum_guide";
    museum_guide.trigger_kind =
        TriggerKind::actor_activation;
    museum_guide.trigger_map = pewter_city.map_id;
    museum_guide.trigger_x =
        pewter_city.museum_actor_index;
    museum_guide.instructions.push_back(
        operation(Opcode::lock_input));
    Instruction museum_question =
        operation(Opcode::ask_yes_no);
    museum_question.pages =
        pewter_city.museum_question.pages;
    museum_guide.instructions.push_back(
        std::move(museum_question));
    const std::size_t museum_no_jump =
        museum_guide.instructions.size();
    museum_guide.instructions.push_back(
        operation(Opcode::jump_if_choice_no));
    museum_guide.instructions.push_back(
        dialogue(pewter_city.museum_yes.pages));
    const std::size_t museum_done_jump =
        museum_guide.instructions.size();
    museum_guide.instructions.push_back(
        operation(Opcode::jump));
    museum_guide.instructions[museum_no_jump].value =
        static_cast<std::uint32_t>(
            museum_guide.instructions.size());
    museum_guide.instructions.push_back(
        dialogue(pewter_city.museum_no.pages));
    museum_guide.instructions.push_back(operation(
        Opcode::escort_player_to,
        pewter_city.museum_actor_index, 2U,
        static_cast<std::uint32_t>(
            pewter_city.museum_player_target.x) |
            static_cast<std::uint32_t>(
                pewter_city.museum_player_target.y)
                << 8U));
    museum_guide.instructions.push_back(
        dialogue(pewter_city.museum_arrival.pages));
    Instruction museum_exit =
        operation(Opcode::actor_path,
                  pewter_city.museum_actor_index,
                  pewter_city.map_id, 3U);
    museum_exit.actor_path.assign(
        4U, PathCommand::down);
    museum_guide.instructions.push_back(
        std::move(museum_exit));
    museum_guide.instructions.push_back(operation(
        Opcode::place_actor,
        pewter_city.museum_actor_index,
        pewter_city.map_id,
        packed_position(
            pewter_city.museum_actor_origin.x,
            pewter_city.museum_actor_origin.y)));
    museum_guide.instructions.push_back(operation(
        Opcode::show_actor,
        pewter_city.museum_actor_index, 0U,
        pewter_city.map_id));
    museum_guide.instructions[museum_done_jump].value =
        static_cast<std::uint32_t>(
            museum_guide.instructions.size());
    museum_guide.instructions.push_back(
        operation(Opcode::unlock_input));
    museum_guide.instructions.push_back(
        operation(Opcode::end));
    programs.push_back(std::move(museum_guide));

    Program repel_guide;
    repel_guide.key = "pewter_city_repel_explanation";
    repel_guide.trigger_kind =
        TriggerKind::actor_activation;
    repel_guide.trigger_map = pewter_city.map_id;
    repel_guide.trigger_x =
        pewter_city.repel_actor_index;
    repel_guide.instructions.push_back(
        operation(Opcode::lock_input));
    Instruction repel_question =
        operation(Opcode::ask_yes_no);
    repel_question.pages =
        pewter_city.repel_question.pages;
    repel_guide.instructions.push_back(
        std::move(repel_question));
    const std::size_t repel_no_jump =
        repel_guide.instructions.size();
    repel_guide.instructions.push_back(
        operation(Opcode::jump_if_choice_no));
    repel_guide.instructions.push_back(
        dialogue(pewter_city.repel_yes.pages));
    const std::size_t repel_done_jump =
        repel_guide.instructions.size();
    repel_guide.instructions.push_back(
        operation(Opcode::jump));
    repel_guide.instructions[repel_no_jump].value =
        static_cast<std::uint32_t>(
            repel_guide.instructions.size());
    repel_guide.instructions.push_back(
        dialogue(pewter_city.repel_no.pages));
    repel_guide.instructions[repel_done_jump].value =
        static_cast<std::uint32_t>(
            repel_guide.instructions.size());
    repel_guide.instructions.push_back(
        operation(Opcode::unlock_input));
    repel_guide.instructions.push_back(
        operation(Opcode::end));
    programs.push_back(std::move(repel_guide));

    const auto gym_escort_instructions = [&]() {
        std::vector<Instruction> escort;
        escort.push_back(operation(Opcode::lock_input));
        escort.push_back(
            dialogue(pewter_city.gym_follow.pages));
        escort.push_back(operation(
            Opcode::escort_player_to,
            pewter_city.gym_guide_actor_index, 3U,
            static_cast<std::uint32_t>(
                pewter_city.gym_player_target.x) |
                static_cast<std::uint32_t>(
                    pewter_city.gym_player_target.y)
                    << 8U));
        escort.push_back(
            dialogue(pewter_city.gym_arrival.pages));
        Instruction exit =
            operation(Opcode::actor_path,
                      pewter_city.gym_guide_actor_index,
                      pewter_city.map_id, 3U);
        exit.actor_path.assign(5U, PathCommand::right);
        escort.push_back(std::move(exit));
        escort.push_back(operation(
            Opcode::place_actor,
            pewter_city.gym_guide_actor_index,
            pewter_city.map_id,
            packed_position(
                pewter_city.gym_guide_actor_origin.x,
                pewter_city.gym_guide_actor_origin.y)));
        escort.push_back(operation(
            Opcode::show_actor,
            pewter_city.gym_guide_actor_index, 0U,
            pewter_city.map_id));
        escort.push_back(
            operation(Opcode::unlock_input));
        escort.push_back(
            operation(Opcode::end));
        return escort;
    };

    Program direct_gym_guide;
    direct_gym_guide.key =
        "pewter_city_gym_guide_activation";
    direct_gym_guide.trigger_kind =
        TriggerKind::actor_activation;
    direct_gym_guide.trigger_map = pewter_city.map_id;
    direct_gym_guide.trigger_x =
        pewter_city.gym_guide_actor_index;
    direct_gym_guide.absent_flag =
        pewter_city.beat_brock_flag;
    direct_gym_guide.instructions =
        gym_escort_instructions();
    programs.push_back(std::move(direct_gym_guide));

    for (std::size_t index = 0U;
         index < pewter_city.east_gate_cells.size(); ++index) {
        Program east_gate;
        east_gate.key =
            "pewter_city_east_gate_" +
            std::to_string(index + 1U);
        east_gate.trigger_kind =
            TriggerKind::player_rectangle;
        east_gate.trigger_map = pewter_city.map_id;
        east_gate.trigger_x =
            pewter_city.east_gate_cells[index].x;
        east_gate.trigger_y =
            pewter_city.east_gate_cells[index].y;
        east_gate.trigger_width = 1U;
        east_gate.trigger_height = 1U;
        east_gate.absent_flag =
            pewter_city.beat_brock_flag;
        east_gate.instructions =
            gym_escort_instructions();
        programs.push_back(std::move(east_gate));
    }

    const auto fossil_nerd_battle_instructions = [&]() {
        std::vector<Instruction> battle;
        battle.push_back(operation(Opcode::lock_input));
        battle.push_back(
            dialogue(mt_moon_fossils.both_mine.pages));
        battle.push_back(operation(
            Opcode::start_trainer_battle,
            mt_moon_fossils.trainer_class, 0U,
            mt_moon_fossils.trainer_party));
        Instruction share =
            operation(Opcode::say_if_player_won);
        share.pages = mt_moon_fossils.okay_share.pages;
        battle.push_back(std::move(share));
        battle.push_back(
            operation(Opcode::end_if_player_lost));
        battle.push_back(operation(
            Opcode::set_flag, 0U, 0U,
            mt_moon_fossils.beat_super_nerd_flag));
        battle.push_back(operation(Opcode::unlock_input));
        battle.push_back(operation(Opcode::end));
        return battle;
    };

    Program fossil_nerd_activation;
    fossil_nerd_activation.key =
        "mt_moon_b2f_super_nerd_battle";
    fossil_nerd_activation.trigger_kind =
        TriggerKind::actor_activation;
    fossil_nerd_activation.trigger_map =
        mt_moon_fossils.map_id;
    fossil_nerd_activation.trigger_x =
        mt_moon_fossils.super_nerd_actor_index;
    fossil_nerd_activation.absent_flag =
        mt_moon_fossils.beat_super_nerd_flag;
    fossil_nerd_activation.instructions =
        fossil_nerd_battle_instructions();
    programs.push_back(std::move(fossil_nerd_activation));

    Program fossil_nerd_gate;
    fossil_nerd_gate.key =
        "mt_moon_b2f_super_nerd_gate";
    fossil_nerd_gate.trigger_kind =
        TriggerKind::player_rectangle;
    fossil_nerd_gate.trigger_map =
        mt_moon_fossils.map_id;
    fossil_nerd_gate.trigger_x =
        mt_moon_fossils.trigger_x;
    fossil_nerd_gate.trigger_y =
        mt_moon_fossils.trigger_y;
    fossil_nerd_gate.trigger_width = 1U;
    fossil_nerd_gate.trigger_height = 1U;
    fossil_nerd_gate.absent_flag =
        mt_moon_fossils.beat_super_nerd_flag;
    fossil_nerd_gate.instructions =
        fossil_nerd_battle_instructions();
    programs.push_back(std::move(fossil_nerd_gate));

    const auto fossil_nerd_response =
        [&](std::string key, std::uint32_t required_flag,
            const DecodedTextProgram& text) {
            Program response;
            response.key = std::move(key);
            response.trigger_kind =
                TriggerKind::actor_activation;
            response.trigger_map =
                mt_moon_fossils.map_id;
            response.trigger_x =
                mt_moon_fossils.super_nerd_actor_index;
            response.required_flag = required_flag;
            response.instructions.push_back(
                operation(Opcode::lock_input));
            response.instructions.push_back(
                dialogue(text.pages));
            response.instructions.push_back(
                operation(Opcode::unlock_input));
            response.instructions.push_back(
                operation(Opcode::end));
            return response;
        };
    programs.push_back(fossil_nerd_response(
        "mt_moon_b2f_super_nerd_after_dome",
        mt_moon_fossils.got_dome_flag,
        mt_moon_fossils.pokemon_lab));
    programs.push_back(fossil_nerd_response(
        "mt_moon_b2f_super_nerd_after_helix",
        mt_moon_fossils.got_helix_flag,
        mt_moon_fossils.pokemon_lab));
    programs.push_back(fossil_nerd_response(
        "mt_moon_b2f_super_nerd_choose_one",
        mt_moon_fossils.beat_super_nerd_flag,
        mt_moon_fossils.each_take_one));

    const auto fossil_choice =
        [&](std::string key, std::uint8_t selected_actor,
            std::uint8_t other_actor, std::uint8_t quantity,
            std::uint16_t item_id, std::uint32_t got_flag,
            const DecodedTextProgram& question,
            const DecodedTextProgram& received,
            PathCommand nerd_step) {
            Program choice;
            choice.key = std::move(key);
            choice.trigger_kind =
                TriggerKind::actor_activation;
            choice.trigger_map = mt_moon_fossils.map_id;
            choice.trigger_x = selected_actor;
            choice.required_flag =
                mt_moon_fossils.beat_super_nerd_flag;
            choice.instructions.push_back(
                operation(Opcode::lock_input));
            Instruction prompt =
                operation(Opcode::ask_yes_no);
            prompt.pages = question.pages;
            choice.instructions.push_back(std::move(prompt));
            choice.instructions.push_back(
                operation(Opcode::end_if_choice_no));
            choice.instructions.push_back(operation(
                Opcode::try_give_item, quantity, 0U, item_id));
            const std::size_t fossil_bag_full_jump =
                choice.instructions.size();
            choice.instructions.push_back(
                operation(Opcode::jump_if_item_grant_failed));
            choice.instructions.push_back(
                dialogue(received.pages));
            choice.instructions.push_back(operation(
                Opcode::hide_actor, selected_actor, 0U,
                mt_moon_fossils.map_id));
            choice.instructions.push_back(operation(
                Opcode::set_flag, 0U, 0U, got_flag));
            Instruction move_nerd =
                operation(Opcode::actor_path,
                          mt_moon_fossils.super_nerd_actor_index,
                          mt_moon_fossils.map_id, 2U);
            move_nerd.actor_path.push_back(nerd_step);
            choice.instructions.push_back(std::move(move_nerd));
            choice.instructions.push_back(
                dialogue(mt_moon_fossils.then_mine.pages));
            choice.instructions.push_back(operation(
                Opcode::hide_actor, other_actor, 0U,
                mt_moon_fossils.map_id));
            choice.instructions.push_back(
                operation(Opcode::unlock_input));
            choice.instructions.push_back(
                operation(Opcode::end));
            choice.instructions[fossil_bag_full_jump].value =
                static_cast<std::uint32_t>(
                    choice.instructions.size());
            choice.instructions.push_back(
                dialogue(mt_moon_fossils.no_room.pages));
            choice.instructions.push_back(
                operation(Opcode::unlock_input));
            choice.instructions.push_back(
                operation(Opcode::end));
            return choice;
        };
    programs.push_back(fossil_choice(
        "mt_moon_b2f_take_dome_fossil",
        mt_moon_fossils.dome_actor_index,
        mt_moon_fossils.helix_actor_index,
        mt_moon_fossils.dome_quantity,
        mt_moon_fossils.dome_item_id,
        mt_moon_fossils.got_dome_flag,
        mt_moon_fossils.dome_question,
        mt_moon_fossils.dome_received,
        PathCommand::right));
    programs.push_back(fossil_choice(
        "mt_moon_b2f_take_helix_fossil",
        mt_moon_fossils.helix_actor_index,
        mt_moon_fossils.dome_actor_index,
        mt_moon_fossils.helix_quantity,
        mt_moon_fossils.helix_item_id,
        mt_moon_fossils.got_helix_flag,
        mt_moon_fossils.helix_question,
        mt_moon_fossils.helix_received,
        PathCommand::up));

    Program magikarp_sale;
    magikarp_sale.key =
        "mt_moon_pokecenter_magikarp_sale";
    magikarp_sale.trigger_kind =
        TriggerKind::actor_activation;
    magikarp_sale.trigger_map =
        mt_moon_magikarp.map_id;
    magikarp_sale.trigger_x =
        mt_moon_magikarp.actor_index;
    magikarp_sale.absent_flag =
        mt_moon_magikarp.bought_flag;
    magikarp_sale.instructions.push_back(
        operation(Opcode::lock_input));
    Instruction magikarp_offer =
        operation(Opcode::ask_yes_no);
    magikarp_offer.pages =
        mt_moon_magikarp.offer.pages;
    magikarp_sale.instructions.push_back(
        std::move(magikarp_offer));
    const std::size_t magikarp_no_jump =
        magikarp_sale.instructions.size();
    magikarp_sale.instructions.push_back(
        operation(Opcode::jump_if_choice_no));
    const std::size_t magikarp_no_money_jump =
        magikarp_sale.instructions.size();
    magikarp_sale.instructions.push_back(operation(
        Opcode::jump_if_money_below,
        static_cast<std::uint8_t>(
            mt_moon_magikarp.price & 0xFFU),
        static_cast<std::uint8_t>(
            mt_moon_magikarp.price >> 8U)));
    magikarp_sale.instructions.push_back(operation(
        Opcode::try_give_pokemon,
        mt_moon_magikarp.level, 0U,
        mt_moon_magikarp.species_dex));
    const std::size_t magikarp_box_full_jump =
        magikarp_sale.instructions.size();
    magikarp_sale.instructions.push_back(
        operation(
            Opcode::jump_if_pokemon_grant_failed));
    magikarp_sale.instructions.push_back(
        species_dialogue(
            mt_moon_magikarp.got_pokemon.pages,
            mt_moon_magikarp.species_dex));
    Instruction magikarp_sent =
        operation(Opcode::say_if_pokemon_sent_to_box);
    magikarp_sent.value =
        mt_moon_magikarp.species_dex;
    magikarp_sent.pages =
        mt_moon_magikarp.sent_to_box.pages;
    magikarp_sale.instructions.push_back(
        std::move(magikarp_sent));
    magikarp_sale.instructions.push_back(operation(
        Opcode::take_money, 0U, 0U,
        mt_moon_magikarp.price));
    magikarp_sale.instructions.push_back(operation(
        Opcode::set_flag, 0U, 0U,
        mt_moon_magikarp.bought_flag));
    magikarp_sale.instructions.push_back(
        operation(Opcode::unlock_input));
    magikarp_sale.instructions.push_back(
        operation(Opcode::end));

    magikarp_sale.instructions[magikarp_no_jump].value =
        static_cast<std::uint32_t>(
            magikarp_sale.instructions.size());
    magikarp_sale.instructions.push_back(
        dialogue(mt_moon_magikarp.chose_no.pages));
    magikarp_sale.instructions.push_back(
        operation(Opcode::unlock_input));
    magikarp_sale.instructions.push_back(
        operation(Opcode::end));

    magikarp_sale.instructions[magikarp_no_money_jump].value =
        static_cast<std::uint32_t>(
            magikarp_sale.instructions.size());
    magikarp_sale.instructions.push_back(
        dialogue(mt_moon_magikarp.no_money.pages));
    magikarp_sale.instructions.push_back(
        operation(Opcode::unlock_input));
    magikarp_sale.instructions.push_back(
        operation(Opcode::end));

    magikarp_sale.instructions[magikarp_box_full_jump].value =
        static_cast<std::uint32_t>(
            magikarp_sale.instructions.size());
    magikarp_sale.instructions.push_back(
        dialogue(mt_moon_magikarp.box_full.pages));
    magikarp_sale.instructions.push_back(
        operation(Opcode::unlock_input));
    magikarp_sale.instructions.push_back(
        operation(Opcode::end));
    programs.push_back(std::move(magikarp_sale));

    Program magikarp_no_refunds;
    magikarp_no_refunds.key =
        "mt_moon_pokecenter_magikarp_no_refunds";
    magikarp_no_refunds.trigger_kind =
        TriggerKind::actor_activation;
    magikarp_no_refunds.trigger_map =
        mt_moon_magikarp.map_id;
    magikarp_no_refunds.trigger_x =
        mt_moon_magikarp.actor_index;
    magikarp_no_refunds.required_flag =
        mt_moon_magikarp.bought_flag;
    magikarp_no_refunds.instructions.push_back(
        operation(Opcode::lock_input));
    magikarp_no_refunds.instructions.push_back(
        dialogue(mt_moon_magikarp.no_refunds.pages));
    magikarp_no_refunds.instructions.push_back(
        operation(Opcode::unlock_input));
    magikarp_no_refunds.instructions.push_back(
        operation(Opcode::end));
    programs.push_back(std::move(magikarp_no_refunds));

    for (const RivalBattleChoice& battle :
         cerulean_rival.battles) {
        Program rival;
        rival.key =
            "cerulean_rival_for_starter_" +
            std::to_string(battle.rival_species);
        rival.trigger_kind =
            TriggerKind::player_rectangle;
        rival.trigger_map = cerulean_rival.map_id;
        rival.trigger_x = cerulean_rival.trigger_x;
        rival.trigger_y = cerulean_rival.trigger_y;
        rival.trigger_width =
            cerulean_rival.trigger_width;
        rival.trigger_height = 1U;
        rival.absent_flag = cerulean_rival.beat_flag;
        rival.required_variable =
            kRivalStarterVariable;
        rival.required_variable_value =
            battle.rival_species;
        rival.instructions.push_back(
            operation(Opcode::lock_input));
        rival.instructions.push_back(operation(
            Opcode::place_actor_at_player_x,
            cerulean_rival.actor_index,
            cerulean_rival.map_id,
            cerulean_rival.approach_start_y));
        rival.instructions.push_back(operation(
            Opcode::show_actor,
            cerulean_rival.actor_index, 0U,
            cerulean_rival.map_id));
        Instruction approach = operation(
            Opcode::actor_path,
            cerulean_rival.actor_index,
            cerulean_rival.map_id, 2U);
        approach.actor_path =
            cerulean_rival.approach;
        rival.instructions.push_back(
            std::move(approach));
        rival.instructions.push_back(operation(
            Opcode::face_player,
            cerulean_rival.actor_index));
        rival.instructions.push_back(
            dialogue(
                cerulean_rival.pre_battle.pages));
        rival.instructions.push_back(operation(
            Opcode::start_trainer_battle,
            battle.trainer_class, 0U,
            battle.trainer_party));
        Instruction defeated =
            operation(Opcode::say_if_player_won);
        defeated.pages =
            cerulean_rival.defeated.pages;
        rival.instructions.push_back(
            std::move(defeated));
        Instruction victory =
            operation(Opcode::say_if_player_lost);
        victory.pages =
            cerulean_rival.victory.pages;
        rival.instructions.push_back(
            std::move(victory));
        const std::size_t won_jump =
            rival.instructions.size();
        rival.instructions.push_back(
            operation(Opcode::jump_if_player_won));
        rival.instructions.push_back(operation(
            Opcode::hide_actor,
            cerulean_rival.actor_index, 0U,
            cerulean_rival.map_id));
        rival.instructions.push_back(
            operation(Opcode::unlock_input));
        rival.instructions.push_back(
            operation(Opcode::end));
        rival.instructions[won_jump].value =
            static_cast<std::uint32_t>(
                rival.instructions.size());
        rival.instructions.push_back(operation(
            Opcode::set_flag, 0U, 0U,
            cerulean_rival.beat_flag));
        rival.instructions.push_back(
            dialogue(
                cerulean_rival.after_battle.pages));
        Instruction exit = operation(
            Opcode::actor_path_by_player_x,
            cerulean_rival.actor_index,
            cerulean_rival.trigger_x,
            cerulean_rival.map_id);
        exit.actor_path =
            cerulean_rival.exit_right;
        exit.player_path =
            cerulean_rival.exit_left;
        rival.instructions.push_back(std::move(exit));
        rival.instructions.push_back(operation(
            Opcode::hide_actor,
            cerulean_rival.actor_index, 0U,
            cerulean_rival.map_id));
        rival.instructions.push_back(
            operation(Opcode::unlock_input));
        rival.instructions.push_back(
            operation(Opcode::end));
        programs.push_back(std::move(rival));
    }

    std::vector<std::uint8_t> cache{'P', 'C', 'P', 'H'};
    write_naming_profile(cache, naming_profile, nickname_heading);
    write_u16(cache, inventory_stack_capacity);
    write_u32(cache, initial_state.money);
    cache.push_back(initial_state.party_capacity);
    cache.push_back(initial_state.storage_box_count);
    write_u16(cache, initial_state.storage_box_capacity);
    write_u16(cache, item_names.size());
    for (const ImportedItemName& item : item_names) {
        write_u16(cache, item.item_id);
        write_string(cache, item.name);
    }
    write_pages(cache, found_item.pages);
    write_pages(cache, no_item_room.pages);
    write_u16(cache, 1U);
    cache.push_back(mt_moon_fossils.map_id);
    cache.push_back(mt_moon_fossils.suppression_x);
    cache.push_back(mt_moon_fossils.suppression_y);
    cache.push_back(mt_moon_fossils.suppression_width);
    cache.push_back(mt_moon_fossils.suppression_height);
    write_u32(
        cache, mt_moon_fossils.beat_super_nerd_flag);
    write_u16(cache, programs.size());
    for (const Program& program : programs) write_program(cache, program);

    result.files.push_back(readable_pallet_source(hey_wait.pages, unsafe.pages, oak_path,
                                                  player_path, lab_oak_path, lab_player_path,
                                                  lab_choice_text));
    result.files.push_back(readable_naming_profile_source(
        naming_profile, nickname_heading, nickname_question));
    for (std::size_t index = 0U; index < starter_choices.size(); ++index)
        result.files.push_back(readable_starter_source(
            starter_keys[index], starter_choices[index],
            starter_prompts[index], starter_energetic,
            player_received, nickname_question, rival_takes,
            rival_received,
            rival_starter_paths[index]));
    for (std::size_t index = 0U; index < rival_battles.size(); ++index)
        result.files.push_back(readable_rival_battle_source(
            rival_battle_keys[index], rival_battles[index],
            rival_challenge, rival_player_victory, rival_player_loss,
            rival_exit_text, rival_exit_path));
    result.files.push_back(
        readable_viridian_mart_parcel_source(parcel));
    result.files.push_back(
        readable_oak_return_source(oak_return));
    for (std::size_t index = 0U;
         index < route_22_first_rival.battles.size();
         ++index)
        result.files.push_back(
            readable_route_22_first_rival_source(
                route_22_rival_keys[index],
                route_22_first_rival,
                route_22_first_rival.battles[index]));
    result.files.push_back(
        readable_oak_pokeball_source(oak_pokeballs));
    result.files.push_back(
        readable_daisy_town_map_source(daisy_town_map));
    result.files.push_back(
        readable_pallet_reward_updates_source(
            pallet_reward_updates));
    result.files.push_back(
        readable_route_1_potion_source(route_1_potion));
    result.files.push_back(
        readable_loose_item_source(
            item_names, found_item, no_item_room));
    result.files.push_back(
        readable_pewter_gym_source(pewter_gym));
    result.files.push_back(
        readable_pewter_city_source(pewter_city));
    result.files.push_back(
        readable_mt_moon_fossil_source(mt_moon_fossils));
    result.files.push_back(
        readable_mt_moon_magikarp_source(
            mt_moon_magikarp));
    result.files.push_back(
        readable_cerulean_rival_source(
            cerulean_rival));
    result.files.push_back(
        readable_initial_actor_visibility_source(toggle_actors));
    result.files.push_back({"compiled/campaign_programs.bin", std::move(cache)});
    result.programs = programs.size();
    error.clear();
    return true;
}

} // namespace pokered::import
