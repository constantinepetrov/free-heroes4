#pragma once
#include <array>
#include <vector>
#include "h4/formats/scenario/records.hpp"
#include "h4/model/creature_catalog.hpp"
#include "h4/rules/battle.hpp"
namespace h4::rules {
[[nodiscard]] core::Result<std::vector<BattleUnit>> battle_army(const formats::ArmyRecord& army, const std::array<int,7>& wounds, int side, const model::CreatureCatalog& creatures);
[[nodiscard]] core::Result<void> apply_battle_losses(formats::ArmyRecord& army, std::array<int,7>& wounds, const std::array<int,7>& counts, const std::array<int,7>& hp);
[[nodiscard]] model::CreatureStats basic_hero_stats(const formats::HeroRecord& hero);
}
