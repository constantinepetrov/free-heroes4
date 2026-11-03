#include "h4/rules/game_state.hpp"
#include <algorithm>
#include <cstdlib>
#include "h4/rules/experience.hpp"
namespace h4::rules {
core::Result<GameState> GameState::resolve_battle(PlayerId player,std::uint32_t attacker,std::uint32_t defender,const Battle& battle,int cost) const {
    const auto own=own_army(player,attacker);if (!own) return std::unexpected{own.error()};
    const ArmyPlace* a=*own;const ArmyPlace* d=army(defender);
    if (!battle.finished() || !d || attacker==defender || d->owner==a->owner || a->at.level!=d->at.level ||
        std::max(std::abs(a->at.x-d->at.x),std::abs(a->at.y-d->at.y))>2 || cost<0 || cost>a->left)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid or unfinished encounter");
    GameState next=*this;
    const auto ac=battle.survivors(0),dc=battle.survivors(1);
    const auto survived=[&](std::uint32_t id) {const auto& c=id==attacker?ac:dc;return std::ranges::any_of(c,[](int n){return n>0;});};
    std::erase_if(next.armies_,[&](const ArmyPlace& p){return (p.actor==attacker || p.actor==defender) && !survived(p.actor);});
    std::erase_if(next.heroes_,[&](const HeroState& h){
        if (h.army!=attacker && h.army!=defender) return false;
        return h.slot>=7 || (h.army==attacker?ac:dc)[h.slot]==0;
    });
    for (auto& p:next.armies_) if (p.actor==attacker) p.left-=cost;
    const int winner=*battle.winner();
    if (winner<2) {
        const std::uint32_t id=winner==0?attacker:defender;
        std::vector<std::int32_t> levels;for (const auto& hero:next.heroes_) if (hero.army==id) levels.push_back(hero.level);
        const auto awards=split_award(battle.experience_award(winner),levels,true);std::size_t i=0;
        for (auto& hero:next.heroes_) if (hero.army==id) {
            hero.experience+=awards[i++];hero.level=std::max(hero.level,level_for_experience(hero.experience));
        }
    }
    return next;
}
}
