#include "h4/rules/battle.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <format>
#include <limits>
#include <queue>
#include <tuple>
namespace h4::rules {
namespace {
bool inside(core::Point p) { return p.x >= 0 && p.y >= 0 && p.x < kBattleWidth && p.y < kBattleHeight; }
int distance(core::Point a, core::Point b) { return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y)); }
int index(core::Point p) { return p.y*kBattleWidth+p.x; }
}
core::Result<Battle> Battle::start(std::vector<BattleUnit> units) {
    if (units.empty() || units.size()>14) return core::fail(core::ErrorCode::invalid_argument,"A battle needs two armies of at most seven units");
    std::array<std::array<bool,7>,2> slots{};
    std::array<bool,2> sides{};
    for (std::size_t i=0;i<units.size();++i) {
        auto& u=units[i];
        if (u.side<0 || u.side>1 || u.slot<0 || u.slot>=7 || slots[static_cast<std::size_t>(u.side)][static_cast<std::size_t>(u.slot)] ||
            u.max_hp<1 || u.max_hp>1000000 || u.initial_count<1 || u.initial_count>32767 || u.hp<1 ||
            u.hp>static_cast<std::int64_t>(u.max_hp)*u.initial_count || u.count()!=u.initial_count ||
            u.attack<1 || u.attack>10000 || u.defense<1 || u.defense>10000 || u.damage_low<0 ||
            u.damage_high<u.damage_low || u.damage_high>1000000 || u.speed<1 || u.speed>10000 ||
            u.movement<1 || u.movement>12 || u.experience<0 || !inside(u.at))
            return core::fail(core::ErrorCode::invalid_argument,"Invalid battle unit");
        for (std::size_t j=0;j<i;++j) if (units[j].at==u.at) return core::fail(core::ErrorCode::invalid_argument,"Units overlap");
        slots[static_cast<std::size_t>(u.side)][static_cast<std::size_t>(u.slot)]=true;
        sides[static_cast<std::size_t>(u.side)]=true;
        u.acted=false;u.retaliated=false;
    }
    if (!sides[0] || !sides[1]) return core::fail(core::ErrorCode::invalid_argument,"Both armies must be present");
    Battle out{std::move(units)};out.next();return out;
}
int Battle::unit_at(core::Point p) const noexcept {
    for (std::size_t i=0;i<units_.size();++i) if (units_[i].hp>0 && units_[i].at==p) return static_cast<int>(i);
    return -1;
}
std::vector<core::Point> Battle::path_to(core::Point to) const {
    if (finished() || !inside(to) || unit_at(to)>=0) return {};
    std::array<int,kBattleWidth*kBattleHeight> previous;previous.fill(-1);
    std::queue<core::Point> open;open.push(active().at);previous[static_cast<std::size_t>(index(active().at))]=index(active().at);
    while (!open.empty()) {
        auto at=open.front();open.pop();
        if (at==to) break;
        for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) {
            if (!dx && !dy) continue;
            core::Point n{at.x+dx,at.y+dy};
            if (!inside(n) || previous[static_cast<std::size_t>(index(n))]>=0 || unit_at(n)>=0) continue;
            if (dx && dy && (unit_at({at.x+dx,at.y})>=0 || unit_at({at.x,at.y+dy})>=0)) continue;
            previous[static_cast<std::size_t>(index(n))]=index(at);open.push(n);
        }
    }
    if (previous[static_cast<std::size_t>(index(to))]<0) return {};
    std::vector<core::Point> path;
    for (int at=index(to);at!=index(active().at);at=previous[static_cast<std::size_t>(at)]) path.push_back({at%kBattleWidth,at/kBattleWidth});
    std::ranges::reverse(path);return path;
}
std::int64_t Battle::damage(const BattleUnit& from,const BattleUnit& to) {
    // shortcut: deterministic midpoint damage and base Attack/Defense only; add seeded rolls and modifiers with special abilities.
    return std::max<std::int64_t>(1,static_cast<std::int64_t>(from.count())*(from.damage_low+from.damage_high)*from.attack/(2LL*to.defense));
}
void Battle::finish_if_needed() {
    bool alive[2]{};
    for (const auto& u:units_) if (u.hp>0) alive[u.side]=true;
    if (!alive[0] || !alive[1]) {
        winner_=alive[0]?0:alive[1]?1:2;
        message_=*winner_==2?"Both armies were destroyed":*winner_==0?"Victory":"Defeat";
    }
}
void Battle::next() {
    finish_if_needed();if (finished()) return;
    auto choose=[&] {
        int best=-1;
        for (std::size_t i=0;i<units_.size();++i) {
            const auto& u=units_[i];if (u.hp<=0 || u.acted) continue;
            if (best<0 || std::tuple{-u.speed,u.slot,u.side}<std::tuple{-units_[static_cast<std::size_t>(best)].speed,units_[static_cast<std::size_t>(best)].slot,units_[static_cast<std::size_t>(best)].side}) best=static_cast<int>(i);
        }
        return best;
    };
    active_=choose();
    if (active_<0) {
        ++round_;for (auto& u:units_) {u.acted=false;u.retaliated=false;}
        active_=choose();
    }
    movement_left_=active().movement;
}
core::Result<void> Battle::apply(const BattleAction& action) {
    if (finished()) return core::fail(core::ErrorCode::invalid_argument,"The battle has finished");
    auto& from=units_[static_cast<std::size_t>(active_)];
    if (action.kind==BattleActionKind::move) {
        auto path=path_to(action.to);
        if (path.empty() || path.size()>static_cast<std::size_t>(movement_left_)) return core::fail(core::ErrorCode::invalid_argument,"Cell is blocked or beyond movement range");
        from.at=action.to;movement_left_-=static_cast<int>(path.size());
        message_=std::format("{} moves to {},{}",from.name,from.at.x,from.at.y);
        return {};
    }
    if (action.kind==BattleActionKind::surrender) {
        for (auto& u:units_) if (u.side==from.side) u.hp=0;
        finish_if_needed();return {};
    }
    if (action.kind==BattleActionKind::attack) {
        if (action.target<0 || static_cast<std::size_t>(action.target)>=units_.size()) return core::fail(core::ErrorCode::invalid_argument,"Invalid target");
        auto& target=units_[static_cast<std::size_t>(action.target)];
        if (target.side==from.side || target.hp<=0 || distance(from.at,target.at)!=1) return core::fail(core::ErrorCode::invalid_argument,"Choose an adjacent enemy");
        const auto dealt=damage(from,target);
        const auto returned=target.retaliated?0:damage(target,from);
        // Heroes IV retaliation uses both pre-strike stacks, even when the defender dies.
        target.hp=std::max<std::int64_t>(0,target.hp-dealt);
        from.hp=std::max<std::int64_t>(0,from.hp-returned);target.retaliated=true;
        message_=std::format("{} hits {}: {} damage, {} retaliation",from.name,target.name,dealt,returned);
    } else if (action.kind==BattleActionKind::defend) {
        message_=from.name+" ends turn";
    } else return core::fail(core::ErrorCode::invalid_argument,"Unknown battle action");
    from.acted=true;next();return {};
}
BattleAction Battle::ai_action() const {
    if (finished()) return {};
    const auto& from=active();
    for (std::size_t i=0;i<units_.size();++i) if (units_[i].hp>0 && units_[i].side!=from.side && distance(from.at,units_[i].at)==1)
        return {BattleActionKind::attack,{},static_cast<int>(i)};
    std::vector<core::Point> best;
    for (const auto& u:units_) if (u.hp>0 && u.side!=from.side) {
        for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) {
            if (!dx && !dy) continue;
            auto path=path_to({u.at.x+dx,u.at.y+dy});
            if (!path.empty() && (best.empty() || path.size()<best.size())) best=std::move(path);
        }
    }
    if (movement_left_<=0 || best.empty()) return {BattleActionKind::defend};
    return {BattleActionKind::move,best[std::min(best.size(),static_cast<std::size_t>(movement_left_))-1]};
}
std::array<int,7> Battle::survivors(int side) const {
    std::array<int,7> out{};for (const auto& u:units_) if (u.side==side) out[static_cast<std::size_t>(u.slot)]=u.count();return out;
}
std::array<int,7> Battle::wounds(int side) const {
    std::array<int,7> out{};for (const auto& u:units_) if (u.side==side && u.hp>0) out[static_cast<std::size_t>(u.slot)]=static_cast<int>((u.hp-1)%u.max_hp+1);return out;
}
std::int64_t Battle::experience_award(int side) const {
    if (winner_!=side) return 0;
    std::int64_t xp=0;for (const auto& u:units_) if (u.side!=side) xp+=static_cast<std::int64_t>(u.initial_count-u.count())*u.experience;return xp;
}
}
