#include "battle_cases.hpp"
#include <stdexcept>
#include <string>
#include "h4/rules/battle.hpp"
#include "h4/rules/battle_army.hpp"
#include "h4/rules/game_state.hpp"
namespace h4::tests {
namespace {
void check(bool value,const char* what) {if (!value) throw std::runtime_error(what);}
#define CHECK(...) check(bool(__VA_ARGS__), #__VA_ARGS__)
rules::BattleUnit unit(int side,int slot,int x,int y,int count=10,int hp=10,int damage=1,int speed=5) {
    rules::BattleUnit u;u.side=side;u.slot=slot;u.name=side?"Defender":"Attacker";u.initial_count=count;u.max_hp=hp;
    u.hp=static_cast<std::int64_t>(count)*hp;u.attack=10;u.defense=10;u.damage_low=damage;u.damage_high=damage;u.speed=speed;u.movement=2;u.at={x,y};u.experience=7;return u;
}
rules::Battle start(std::vector<rules::BattleUnit> units) {auto b=rules::Battle::start(std::move(units));CHECK(b);return std::move(*b);}
rules::GameState world(bool friendly=false,int enemy_x=5) {
    rules::GameStart s;s.players=2;s.armies={{1,rules::PlayerId{0},{4,4,0,false},2400,2400},{2,friendly?std::optional{rules::PlayerId{0}}:std::nullopt,{enemy_x,4,0,false},1000,1000}};
    rules::HeroState hero;hero.army=1;hero.slot=0;hero.experience=0;hero.level=1;s.heroes.push_back(hero);
    auto out=rules::GameState::start(std::move(s));CHECK(out);return *out;
}
}
void battle_checks() {
    using namespace rules;
    {
        auto a=unit(0,0,1,1),d=unit(1,0,1,1);
        CHECK(!Battle::start({a,d}));d.at={2,1};a.max_hp=0;CHECK(!Battle::start({a,d}));
        a=unit(0,0,1,1);a.hp=101;CHECK(!Battle::start({a,d}));
        CHECK(!Battle::start({a}));
    }
    {
        auto b=start({unit(0,0,1,1,10,10,1,5),unit(1,0,2,1,10,10,1,6)});
        CHECK(b.active_index()==1);CHECK(b.apply({BattleActionKind::defend}));CHECK(b.active_index()==0);
        CHECK(b.round()==1);CHECK(b.apply({BattleActionKind::defend}));CHECK(b.active_index()==1);CHECK(b.round()==2);
    }
    {
        auto a=unit(0,0,1,1),d=unit(1,0,2,1,10,10,1,1);a.attack=20;a.damage_low=2;a.damage_high=4;d.attack=5;
        auto b=start({a,d});CHECK(Battle::damage(a,d)==60);
        CHECK(b.apply({BattleActionKind::attack,{},1}));CHECK(b.units()[0].hp==95);CHECK(b.units()[1].hp==40);
        CHECK(b.survivors(1)[0]==4);CHECK(b.wounds(0)[0]==5);CHECK(b.wounds(1)[0]==10);
    }
    {
        auto b=start({unit(0,0,1,1,10,10,1,6),unit(0,1,1,2,10,10,1,5),unit(1,0,2,1,10,10,1,1)});
        CHECK(b.apply({BattleActionKind::attack,{},2}));CHECK(b.units()[0].hp==90);
        CHECK(b.active_index()==1);CHECK(b.apply({BattleActionKind::attack,{},2}));
        CHECK(b.units()[1].hp==100);CHECK(b.units()[2].hp==80);
    }
    {
        auto b=start({unit(0,0,1,1,1,10,10),unit(1,0,2,1,1,10,10,1)});
        CHECK(b.apply({BattleActionKind::attack,{},1}));CHECK(b.winner()==2);CHECK(b.units()[0].hp==0);CHECK(b.units()[1].hp==0);
        CHECK(!b.apply({BattleActionKind::defend}));CHECK(b.experience_award(0)==0);
    }
    {
        auto b=start({unit(0,0,1,1),unit(1,0,10,1,10,10,1,1)});
        CHECK(!b.apply({BattleActionKind::attack,{},0}));CHECK(!b.apply({BattleActionKind::attack,{},1}));
        CHECK(!b.apply({BattleActionKind::move,{4,1}}));CHECK(b.active().at==core::Point(1,1));
        CHECK(b.apply({BattleActionKind::move,{3,1}}));CHECK(b.movement_left()==0);
        CHECK(!b.apply({BattleActionKind::move,{4,1}}));CHECK(b.active().at==core::Point(3,1));
        CHECK(b.apply({BattleActionKind::defend}));CHECK(b.active().side==1);
    }
    {
        auto hero=unit(0,0,1,1,1,100,100);hero.hero=true;auto d=unit(1,0,2,1,2,5,1,1);
        auto b=start({hero,unit(0,1,1,2,3,10,1,1),d});CHECK(b.apply({BattleActionKind::attack,{},2}));CHECK(b.winner()==0);
        CHECK(b.wounds(0)[0]==98);CHECK(b.experience_award(0)==14);
        auto state=world();auto result=state.resolve_battle(PlayerId{0},1,2,b,100);CHECK(result);
        CHECK(result->army(1)!=nullptr);CHECK(result->army(1)->left==2300);CHECK(result->army(2)==nullptr);
        CHECK(result->heroes().size()==1);CHECK(result->heroes()[0].experience==14);
        CHECK(!result->resolve_battle(PlayerId{0},1,2,b,100));
        CHECK(!state.resolve_battle(PlayerId{1},1,2,b,100));CHECK(!state.resolve_battle(PlayerId{0},1,2,b,-1));
        CHECK(!world(true).resolve_battle(PlayerId{0},1,2,b,100));CHECK(!world(false,20).resolve_battle(PlayerId{0},1,2,b,100));
        formats::ArmyRecord army;army.slots[0].kind=formats::ArmySlot::Kind::hero;army.slots[1].kind=formats::ArmySlot::Kind::creature;army.slots[1].stack.count=3;army.slots[1].stack.creature=1;
        std::array<int,7> wounds{};auto invalid=b.survivors(0);invalid[1]=4;
        CHECK(!apply_battle_losses(army,wounds,invalid,b.wounds(0)));CHECK(army.slots[1].stack.count==3);
        CHECK(apply_battle_losses(army,wounds,b.survivors(0),b.wounds(0)));CHECK(wounds[0]==98);CHECK(army.slots[1].stack.count==3);
    }
    {
        auto b=start({unit(0,0,1,1,1,10,1,1),unit(1,0,2,1,1,10,50,10)});
        CHECK(b.apply({BattleActionKind::attack,{},0}));CHECK(b.winner()==1);
        auto result=world().resolve_battle(PlayerId{0},1,2,b,100);CHECK(result);CHECK(!result->army(1));CHECK(result->army(2));CHECK(result->heroes().empty());
    }
    {
        auto b=start({unit(0,0,1,1),unit(1,0,10,6,2,10,1,1)});int actions=0;
        while (!b.finished() && actions<300) {CHECK(b.apply(b.ai_action()));++actions;}
        CHECK(b.finished());CHECK(b.winner()==0);CHECK(actions<100);
    }
    {
        formats::HeroRecord h;h.level=3;const auto s=basic_hero_stats(h);
        CHECK(s.hit_points.value()==120);CHECK(s.damage.low==19);CHECK(s.damage.high==28);CHECK(s.attack==10);CHECK(s.defense==10);
    }
}
}
