#include "h4/app/adventure/adventure_controller.hpp"
#include <algorithm>
#include <format>
#include "h4/app/adventure/adventure_panel.hpp"
#include "h4/assets/assets.hpp"
#include "h4/model/shroud.hpp"
#include "h4/rules/battle_army.hpp"
namespace h4::app {
void AdventureController::begin_battle(std::uint32_t defender,const rules::PathField::PathStep& step) {
    if (battle_ || !game_ || !me_ || !selected_ || !database_ || !moves_ || !moves_->hostile(defender)) return;
    const auto id=scenario_.map().index_of(selected_);if (!id) return;
    const auto actors=scenario_.map().actors();if (defender>=actors.size()) return;
    auto a=rules::battle_army(selected_->army,selected_->wounds,0,database_->creatures());
    auto d=rules::battle_army(actors[defender].army,actors[defender].wounds,1,database_->creatures());
    if (report_.failed(a) || report_.failed(d)) return;
    a->insert(a->end(),d->begin(),d->end());auto battle=rules::Battle::start(std::move(*a));
    if (report_.failed(battle)) return;
    encounter_={*id,defender,journey_.uncharged(step.total)};battle_=std::move(*battle);last_battle_tick_=0;
    report_(std::format("battle.begin attacker={} defender={} cost={}",*id,defender,encounter_.cost));show_battle();log_battle();
}
void AdventureController::show_battle() {
    if (!battle_) return;
    const auto* font=assets_->font_for_size(view_->frame_size().height<800?16:20);if (!font) {report_("Missing battle font");return;}
    std::vector<const formats::Layer*> faces;
    for (const auto& u:battle_->units()) {
        const auto& actor=scenario_.map().actors()[u.side==0?encounter_.attacker:encounter_.defender];
        const auto& slot=actor.army.slots[static_cast<std::size_t>(u.slot)];
        faces.push_back(u.hero?hero_face(*assets_,slot.hero,82):creature_face(*assets_,static_cast<std::int16_t>(u.kind),82));
    }
    auto screen=std::make_unique<screens::BattleView>(view_->frame_size(),*font,*battle_,std::move(faces));
    const auto box=screen->confirm_rect();const auto tile=screen->cell_rect({0,0});
    report_(std::format("battle.layout width={} height={} cell_x={} cell_y={} tile={} confirm_x={} confirm_y={}",view_->frame_size().width,view_->frame_size().height,tile.origin().x,tile.origin().y,tile.width(),box.origin().x+box.width()/2,box.origin().y+box.height()/2));
    battle_view_.open(std::move(screen),[this](auto& opened,auto& connections){
        connections.on(opened.action,[this](rules::BattleAction action){battle_action(action);});
        connections.on(opened.returned,[this]{finish_battle();});
    });
}
void AdventureController::log_battle() const {
    if (!battle_) return;
    report_(std::format("battle.state round={} active={} move={} finished={}",battle_->round(),battle_->active_index(),battle_->movement_left(),battle_->finished()));
    for (std::size_t i=0;i<battle_->units().size();++i) {
        const auto& u=battle_->units()[i];report_(std::format("battle.unit id={} side={} slot={} at={},{} count={} hp={} name={}",i,u.side,u.slot,u.at.x,u.at.y,u.count(),u.hp,u.name));
    }
}
void AdventureController::battle_action(const rules::BattleAction& action) {
    if (!battle_ || battle_->finished() || battle_->active().side!=0) return;
    const auto applied=battle_->apply(action);
    if (battle_view_.get()) battle_view_.get()->refresh(applied?std::string{}:applied.error().message());
    if (applied) {report_("battle.action "+battle_->message());log_battle();}
}
void AdventureController::battle_tick(std::uint32_t now) {
    if (!battle_ || battle_->finished() || battle_->active().side==0 || now-last_battle_tick_<350) return;
    last_battle_tick_=now;const auto applied=battle_->apply(battle_->ai_action());
    if (report_.failed(applied)) return;
    if (battle_view_.get()) battle_view_.get()->refresh();
    report_("battle.ai "+battle_->message());log_battle();
}
void AdventureController::finish_battle() {
    if (!battle_ || !battle_->finished() || !game_ || !me_) return;
    auto next=game_->resolve_battle(*me_,encounter_.attacker,encounter_.defender,*battle_,encounter_.cost);
    if (report_.failed(next)) return;
    const auto actors=scenario_.map().actors();auto a=actors[encounter_.attacker].army,d=actors[encounter_.defender].army;
    const auto ac=battle_->survivors(0),dc=battle_->survivors(1),ah=battle_->wounds(0),dh=battle_->wounds(1);
    auto a_wounds=actors[encounter_.attacker].wounds,d_wounds=actors[encounter_.defender].wounds;
    if (report_.failed(rules::apply_battle_losses(a,a_wounds,ac,ah)) || report_.failed(rules::apply_battle_losses(d,d_wounds,dc,dh))) return;
    for (const auto& hero:next->heroes()) if (hero.slot<7) {
        if (hero.army==encounter_.attacker) a.slots[hero.slot].hero.level=static_cast<std::int8_t>(hero.level);
        if (hero.army==encounter_.defender) d.slots[hero.slot].hero.level=static_cast<std::int8_t>(hero.level);
    }
    const int ar=model::sight_radius(actors[encounter_.attacker],{1,1}),dr=model::sight_radius(actors[encounter_.defender],{1,1});
    const auto applied=scenario_.map().replace_battle_armies(encounter_.attacker,std::move(a),a_wounds,encounter_.defender,std::move(d),d_wounds);
    if (report_.failed(applied)) return;
    std::int64_t awarded_xp=0;
    for (const auto& hero:next->heroes()) if (hero.army==encounter_.attacker) {
        for (const auto& before:game_->heroes()) if (before.army==hero.army && before.slot==hero.slot) awarded_xp+=hero.experience-before.experience;
    }
    game_=std::move(*next);
    for (const auto [id,radius]:{std::pair{encounter_.attacker,ar},std::pair{encounter_.defender,dr}}) {
        if (!game_->army(id)) {
            if (shroud_) shroud_->army_defeated(id,radius);
        }
    }
    const std::string result=std::format("{}; experience {}",battle_->message(),awarded_xp);
    report_("battle.result "+result);
    for (const auto& hero:game_->heroes()) if (hero.army==encounter_.attacker || hero.army==encounter_.defender)
        report_(std::format("battle.hero army={} slot={} xp={} level={}",hero.army,hero.slot,hero.experience,hero.level));
    battle_view_.close();battle_.reset();journey_=Journey{};path_.clear();
    selected_=nullptr;show_map();
    if (game_->army(encounter_.attacker)) {
        select(&scenario_.map().actors()[encounter_.attacker]);
        centre_on_actor(*selected_);
    }
    show_heroes();present_map();
    screens::BasicDialogContent content;content.title="Battle result";content.text=result;messages_.add(std::move(content),{});messages_.show();
}
}
