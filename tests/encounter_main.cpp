#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>
#include "h4/app/adventure/adventure_controller.hpp"
#include "h4/app/boot/game_opening.hpp"
#include "h4/assets/asset_library.hpp"
#include "h4/graphics/canvas.hpp"
#include "h4/io/archive_chain.hpp"
#include "h4/io/png_writer.hpp"
#include "h4/model/game_database.hpp"
namespace {
template<class T> T take(h4::core::Result<T> result) {if (!result) throw std::runtime_error(result.error().message());return std::move(*result);}
void check(bool ok,const char* message) {if (!ok) throw std::runtime_error(message);}
void tap(h4::screens::View& view,h4::core::Rect box) {
    using namespace h4::platform::events;auto p=box.origin()+h4::core::Point{box.width()/2,box.height()/2};
    view.handle(PointerMoved{p});view.handle(PointerPressed{p,h4::platform::PointerButton::primary,false});view.handle(PointerReleased{p,h4::platform::PointerButton::primary});
}
void shot(h4::app::AdventureController& c,const std::filesystem::path& path) {
    h4::graphics::Surface565 s{c.frame().frame_size()};c.view().paint(h4::graphics::Canvas{s});auto rgba=s.to_rgba();
    auto result=h4::io::write_png(path,s.size(),rgba);if (!result) throw std::runtime_error(result.error().message());
}
}
int main(int argc,char** argv) {try {
    using namespace h4;
    if (argc!=4) {std::cerr<<"usage: h4-encounter-test <data> <map.h4c> <output-dir>\n";return 2;}
    const std::filesystem::path out{argv[3]};std::filesystem::create_directories(out);
    auto chain=take(io::ArchiveChain::mount_installation(argv[1]));auto database=take(model::GameDatabase::load(chain));assets::AssetLibrary assets{chain};
    auto scenario=take(model::Scenario::load(argv[2]));auto setup=model::ScenarioSetup::fill(scenario.summary(),"Player","Computer");
    auto opening=take(app::resolve_and_open(std::move(scenario),std::move(setup),&database));
    auto screen=take(screens::AdventureView::build(assets,{1280,1024}));
    app::AdventureController c{std::move(screen),assets,std::move(opening),{},[](std::string_view s){std::cout<<s<<'\n';},&database};
    std::uint32_t tick=0;
    auto advance=[&]{tick+=40;c.view().advance(core::Instant{std::chrono::milliseconds{tick}});};
    for (int i=0;i<100 && c.message_open();++i) {tap(c.view(),c.message()->button_rect());advance();}
    auto actors=c.scenario().map().actors();const rules::ArmyPlace* own=nullptr;
    for (const auto& army:c.game()->armies()) if (army.owner==rules::PlayerId{0} && actors[army.actor].army.leader()) {own=&army;break;}
    check(own,"No player army");const auto from=own->at;
    std::vector<std::pair<int,std::uint32_t>> enemies;
    for (const auto& army:c.game()->armies()) if (!army.owner && army.at.level==from.level)
        enemies.emplace_back(std::abs(army.at.x-from.x)+std::abs(army.at.y-from.y),army.actor);
    std::ranges::sort(enemies);
    shot(c,out/"adventure-before.png");
    for (const auto [distance,id]:enemies) {
        if (distance>30) break;const auto& target=actors[id];
        const auto pin=c.frame().map_window().pin_in_root(target.position,target.level);
        if (pin) std::cout<<"candidate actor="<<id<<" tile="<<target.position.x<<','<<target.position.y<<" pin="<<pin->x<<','<<pin->y<<'\n';
        scene::MapHover hover{{},target.level,target.position,id};
        c.frame().map_window().clicked.emit(hover);c.frame().map_window().clicked.emit(hover);
        for (int i=0;i<2000 && !c.battle();++i) {advance();if (c.message_open()) tap(c.view(),c.message()->button_rect());if (!c.walking()) break;}
        if (c.battle()) break;
    }
    check(c.battle(),"No reachable enemy produced a battle");const auto [attacker,defender]=c.battle_armies();
    std::cout<<"encounter attacker="<<attacker<<" defender="<<defender<<'\n';shot(c,out/"battle-start.png");
    int turns=0;
    while (c.battle() && !c.battle()->finished() && turns<2000) {
        if (c.battle()->active().side==0) {
            auto action=c.battle()->ai_action();const auto* view=c.battle_screen();check(view,"Missing battle view");
            if (action.kind==rules::BattleActionKind::defend) tap(c.view(),view->end_turn_rect());
            else {
                const auto target=action.kind==rules::BattleActionKind::attack?c.battle()->units()[static_cast<std::size_t>(action.target)].at:action.to;
                tap(c.view(),view->cell_rect(target));tap(c.view(),view->confirm_rect());
            }
        }
        advance();++turns;
    }
    check(c.battle() && c.battle()->finished(),"Battle did not finish");const auto winner=*c.battle()->winner();const auto counts0=c.battle()->survivors(0),counts1=c.battle()->survivors(1);
    std::cout<<"finished winner="<<winner<<" actions="<<turns<<" xp="<<c.battle()->experience_award(0)<<'\n';shot(c,out/"battle-result.png");
    tap(c.view(),c.battle_screen()->confirm_rect());advance();check(!c.battle(),"Result did not return to adventure");
    check((c.game()->army(defender)==nullptr)==(winner!=1),"Defender world state mismatch");
    check((c.game()->army(attacker)==nullptr)==(winner!=0),"Attacker world state mismatch");
    actors=c.scenario().map().actors();
    for (int side=0;side<2;++side) {
        const auto& army=actors[side==0?attacker:defender];const auto counts=side==0?counts0:counts1;
        for (std::size_t i=0;i<7;++i) {
            const auto& slot=army.army.slots[i];const int count=slot.vacant()?0:slot.kind==formats::ArmySlot::Kind::hero?1:slot.stack.count;
            check(count==counts[i],"Map casualties mismatch");
        }
        check(army.removed==(side!=winner),"Defeated map army remains visible");
    }
    if (c.message_open()) tap(c.view(),c.message()->button_rect());advance();shot(c,out/"adventure-after.png");
    std::cout<<"PASS: original scenario -> movement -> battle UI -> losses/XP -> adventure\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
