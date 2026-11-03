#include "h4/screens/battle/battle_view.hpp"
#include <algorithm>
#include <format>
#include <variant>
#include "h4/formats/bitmap_group.hpp"
#include "h4/formats/font.hpp"
#include "h4/graphics/canvas.hpp"
namespace h4::screens {
namespace {
constexpr graphics::Rgba ink{255,239,196,255}, gold{198,155,68,255}, dark{31,27,22,255};
}
BattleView::BattleView(core::Size frame,const formats::Font& font,const rules::Battle& battle,std::vector<const formats::Layer*> faces)
:frame_{frame},font_{&font},battle_{&battle},faces_{std::move(faces)} {
    tile_=std::max(24,std::min((frame.width-236)/rules::kBattleWidth,(frame.height-196)/rules::kBattleHeight));
    panel_=24+tile_*rules::kBattleWidth;refresh();
}
core::Rect BattleView::cell(core::Point at) const {return {{16+at.x*tile_,68+at.y*tile_},{tile_,tile_}};}
core::Rect BattleView::button(int row) const {
    const int height=frame_.height<700?52:72,step=height+8;
    return {{panel_,frame_.height-(3*step+16)+row*step},{frame_.width-panel_-16,height}};
}
void BattleView::refresh(std::string error) {
    pending_.reset();path_.clear();inspected_=battle_->active_index();
    words_=error.empty()?"Tap a cell or adjacent enemy, then Confirm. End turn when done.":std::move(error);
}
void BattleView::handle(const platform::InputEvent& e) {
    using namespace platform;
    if (const auto* p=std::get_if<events::PointerPressed>(&e);p && p->button==PointerButton::primary) pressed_=p->position;
    const auto* up=std::get_if<events::PointerReleased>(&e);
    if (!up || up->button!=PointerButton::primary || !pressed_) return;
    const auto down=*pressed_;pressed_.reset();
    if (button(0).contains(up->position) && button(0).contains(down)) {
        if (battle_->finished()) returned.emit();
        else if (pending_ && battle_->active().side==0) {const auto chosen=*pending_;action.emit(chosen);}
        return;
    }
    if (battle_->finished() || battle_->active().side!=0) return;
    if (button(1).contains(up->position) && button(1).contains(down)) {action.emit({rules::BattleActionKind::defend});return;}
    if (button(2).contains(up->position) && button(2).contains(down)) {
        pending_=rules::BattleAction{rules::BattleActionKind::surrender};words_="Surrender destroys this army. Confirm to surrender.";return;
    }
    for (int y=0;y<rules::kBattleHeight;++y) for (int x=0;x<rules::kBattleWidth;++x) {
        core::Point at{x,y};if (!cell(at).contains(up->position) || !cell(at).contains(down)) continue;
        const int u=battle_->unit_at(at);pending_.reset();path_.clear();
        if (u>=0) {
            inspected_=u;const auto& target=battle_->units()[static_cast<std::size_t>(u)];
            if (target.side==0) words_=u==battle_->active_index()?"This unit acts now. Choose a destination or enemy.":"Inspection only: wait for this unit's turn.";
            else {
                pending_=rules::BattleAction{rules::BattleActionKind::attack,{},u};
                words_=std::format("Attack {}. Adjacent targets only. Confirm attack.",target.name);
            }
        } else {
            pending_=rules::BattleAction{rules::BattleActionKind::move,at};path_=battle_->path_to(at);
            words_=std::format("Move to {},{}: {} / {} cells. Confirm move.",x,y,path_.size(),battle_->movement_left());
        }
        return;
    }
}
void BattleView::paint(const graphics::Canvas& c) const {
    c.fill_rect({{},frame_},dark);
    const graphics::TextStyle text{ink,{}};
    const auto label=[&](std::string_view s,core::Rect box){c.draw_text_block(*font_,s,box,text,{});};
    label(std::format("BATTLE - round {} | Basic melee rules",battle_->round()),{{20,16},{frame_.width-40,42}});
    for (int y=0;y<rules::kBattleHeight;++y) for (int x=0;x<rules::kBattleWidth;++x) {
        const auto box=cell({x,y});c.fill_rect(box,(x+y)%2?graphics::Rgba{63,66,42,255}:graphics::Rgba{54,58,35,255});c.stroke_rect(box,{95,91,58,255});
    }
    for (const auto& p:path_) c.stroke_rect(cell(p),{90,210,140,255});
    for (std::size_t i=0;i<battle_->units().size();++i) {
        const auto& u=battle_->units()[i];if (u.hp<=0) continue;auto box=cell(u.at);
        c.stroke_rect(box,u.side?graphics::Rgba{225,89,72,255}:graphics::Rgba{73,184,215,255});
        if (static_cast<int>(i)==battle_->active_index() && !battle_->finished()) {
            c.stroke_rect({box.origin()+core::Point{2,2},{tile_-4,tile_-4}},gold);
        }
        if (i<faces_.size() && faces_[i]) {
            const auto* face=faces_[i];const auto size=face->bounds().size();
            const double scale=std::min(static_cast<double>(tile_-14)/std::max(1,size.width),static_cast<double>(tile_-26)/std::max(1,size.height));
            c.draw_layer_scaled(*face,box.origin()+core::Point{7,4}-face->bounds().origin(),std::min(1.0,scale));
        }
        label(std::to_string(u.count()),{box.origin()+core::Point{4,tile_-22},{tile_-8,22}});
    }
    if (!battle_->finished()) {
        const auto& u=battle_->active();label(std::format("{} turn\n{}\nMove: {}",u.side==0?"Your":"Enemy",u.name,battle_->movement_left()),{{panel_,68},{frame_.width-panel_-12,104}});
    } else label(std::format("{}\nExperience: {}",battle_->message(),battle_->experience_award(0)),{{panel_,68},{frame_.width-panel_-12,104}});
    if (inspected_>=0 && static_cast<std::size_t>(inspected_)<battle_->units().size()) {
        const auto& u=battle_->units()[static_cast<std::size_t>(inspected_)];
        label(std::format("{}\nCount: {}\nHP: {} / {}\nAttack / Defense: {} / {}\nDamage: {}-{}\nSpeed: {}",u.name,u.count(),u.hp,static_cast<std::int64_t>(u.max_hp)*u.initial_count,u.attack,u.defense,u.damage_low,u.damage_high,u.speed),{{panel_,180},{frame_.width-panel_-12,std::min(270,button(0).origin().y-192)}});
    }
    label(words_,{{20,frame_.height-116},{panel_-30,56}});
    for (int i=0;i<(battle_->finished()?1:3);++i) {
        const auto box=button(i);c.fill_rect(box,{91,63,29,255});c.stroke_rect(box,gold);
        const std::string caption=i==0?(battle_->finished()?"Return to map":pending_?"Confirm":"Choose action"):i==1?"End turn":"Surrender...";
        c.draw_text_block(*font_,caption,box,text,{graphics::HorizontalAlign::centre,graphics::VerticalAlign::centre});
    }
    label(battle_->message(),{{20,frame_.height-54},{panel_-30,48}});
}
}
