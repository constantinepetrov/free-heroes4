#include "h4/rules/battle_army.hpp"
#include <algorithm>
#include <array>
#include "h4/model/skills.hpp"
#include "h4/rules/army_slots.hpp"
namespace h4::rules {
model::CreatureStats basic_hero_stats(const formats::HeroRecord& h) {
    constexpr std::array<int,6> attack{10,15,20,30,40,40},defense{10,15,20,30,40,60};
    const int level=model::hero_level(h);
    model::CreatureStats out{};
    out.hit_points=model::HitPoints{90+10*level};out.damage={16+16*(level-1)/10,24+24*(level-1)/10};
    out.attack=attack[static_cast<std::size_t>(hero_rank(h,model::Discipline::combat)+1)]+h.additions[0];
    out.defense=defense[static_cast<std::size_t>(hero_rank(h,model::Discipline::toughness)+1)]+h.additions[1];
    out.speed=6+h.additions[2];out.movement=16;
    return out;
}
core::Result<std::vector<BattleUnit>> battle_army(const formats::ArmyRecord& army,const std::array<int,7>& wounds,int side,const model::CreatureCatalog& creatures) {
    if (side<0 || side>1) return core::fail(core::ErrorCode::invalid_argument,"Invalid army side");
    std::vector<BattleUnit> units;
    for (std::size_t i=0;i<army.slots.size();++i) {
        const auto& slot=army.slots[i];if (slot.vacant()) continue;
        BattleUnit u;u.side=side;u.slot=static_cast<int>(i);u.hero=slot.kind==formats::ArmySlot::Kind::hero;
        model::CreatureStats stats{};
        if (u.hero) {u.kind=slot.hero.id;u.name=slot.hero.name;stats=basic_hero_stats(slot.hero);u.initial_count=1;}
        else {
            const auto* type=slot_creature(slot,creatures);
            if (!type) return core::fail(core::ErrorCode::unsupported,"Unknown creature in battle army");
            u.kind=slot.stack.creature;u.name=std::string{type->name()};stats=type->stats();u.initial_count=slot.stack.count;
        }
        u.max_hp=stats.hit_points.value();u.attack=stats.attack;u.defense=stats.defense;
        u.damage_low=stats.damage.low;u.damage_high=stats.damage.high;u.speed=stats.speed;
        // shortcut: one coarse board cell represents four movement points; replace with retail-scale terrain/pathfinding later.
        u.movement=std::clamp((stats.movement+3)/4,1,6);u.experience=stats.experience.value();
        const int top=wounds[i]>0?std::min(wounds[i],u.max_hp):u.max_hp;
        u.hp=static_cast<std::int64_t>(u.initial_count-1)*u.max_hp+top;
        u.at={side==0?1:kBattleWidth-2,static_cast<int>(i)+1};units.push_back(std::move(u));
    }
    return units;
}
core::Result<void> apply_battle_losses(formats::ArmyRecord& army,std::array<int,7>& wounds,const std::array<int,7>& counts,const std::array<int,7>& hp) {
    for (std::size_t i=0;i<7;++i) {
        const auto& slot=army.slots[i];const int before=slot.vacant()?0:slot.kind==formats::ArmySlot::Kind::hero?1:slot.stack.count;
        if (counts[i]<0 || counts[i]>before || hp[i]<0 || (counts[i]>0 && hp[i]==0) || (counts[i]==0 && hp[i]!=0))
            return core::fail(core::ErrorCode::invalid_argument,"Invalid battle survivors");
    }
    for (std::size_t i=0;i<7;++i) {
        auto& slot=army.slots[i];
        if (counts[i]==0) slot.kind=formats::ArmySlot::Kind::empty;
        else if (slot.kind==formats::ArmySlot::Kind::creature) slot.stack.count=static_cast<std::int16_t>(counts[i]);
    }
    wounds=hp;return {};
}
}
