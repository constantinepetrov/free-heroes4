#pragma once
#include <optional>
#include <vector>
#include "h4/core/signal.hpp"
#include "h4/screens/shell/popup.hpp"
#include "h4/rules/battle.hpp"
namespace h4::formats {class Font;class Layer;}
namespace h4::screens {
class BattleView final : public Popup {
public:
    BattleView(core::Size frame,const formats::Font& font,const rules::Battle& battle,std::vector<const formats::Layer*> faces);
    void handle(const platform::InputEvent& event) override;
    void paint(const graphics::Canvas& canvas) const override;
    [[nodiscard]] bool opaque() const noexcept override {return true;}
    [[nodiscard]] bool holds(core::Point) const override {return true;}
    [[nodiscard]] bool covers(core::Point) const override {return true;}
    void refresh(std::string error = {});
    [[nodiscard]] core::Rect cell_rect(core::Point at) const { return cell(at); }
    [[nodiscard]] core::Rect confirm_rect() const { return button(0); }
    [[nodiscard]] core::Rect end_turn_rect() const { return button(1); }
    core::Signal<rules::BattleAction> action;
    core::Signal<> returned;
private:
    core::Rect cell(core::Point at) const;
    core::Rect button(int row) const;
    core::Size frame_;
    int tile_ = 44, panel_ = 0;
    const formats::Font* font_;
    const rules::Battle* battle_;
    std::vector<const formats::Layer*> faces_;
    std::optional<core::Point> pressed_;
    std::optional<rules::BattleAction> pending_;
    std::vector<core::Point> path_;
    std::string words_;
    int inspected_ = -1;
};
}
