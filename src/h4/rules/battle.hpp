#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "h4/core/error.hpp"
#include "h4/core/geometry.hpp"
namespace h4::rules {
inline constexpr int kBattleWidth = 12, kBattleHeight = 9;
struct BattleUnit {
    int side = 0, slot = 0;
    std::string name;
    int kind = -1; // Creature id, or hero id when hero is true.
    bool hero = false;
    int max_hp = 1, attack = 1, defense = 1, damage_low = 1, damage_high = 1;
    int speed = 1, movement = 3, experience = 0;
    int initial_count = 1;
    std::int64_t hp = 1;
    core::Point at;
    bool acted = false, retaliated = false;
    [[nodiscard]] int count() const noexcept { return static_cast<int>((hp + max_hp - 1) / max_hp); }
};
enum class BattleActionKind { move, attack, defend, surrender };
struct BattleAction {
    BattleActionKind kind; core::Point to; int target;
    BattleAction(BattleActionKind action=BattleActionKind::defend,core::Point cell={},int unit=-1) : kind{action},to{cell},target{unit} {}
};
class Battle {
public:
    [[nodiscard]] static core::Result<Battle> start(std::vector<BattleUnit> units);
    [[nodiscard]] const std::vector<BattleUnit>& units() const noexcept { return units_; }
    [[nodiscard]] const BattleUnit& active() const { return units_[static_cast<std::size_t>(active_)]; }
    [[nodiscard]] int active_index() const noexcept { return active_; }
    [[nodiscard]] int round() const noexcept { return round_; }
    [[nodiscard]] bool finished() const noexcept { return winner_.has_value(); }
    // 0/1 is the surviving side; 2 is mutual destruction.
    [[nodiscard]] std::optional<int> winner() const noexcept { return winner_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] int unit_at(core::Point p) const noexcept;
    [[nodiscard]] std::vector<core::Point> path_to(core::Point to) const;
    [[nodiscard]] int movement_left() const noexcept { return movement_left_; }
    [[nodiscard]] core::Result<void> apply(const BattleAction& action);
    [[nodiscard]] BattleAction ai_action() const;
    [[nodiscard]] std::array<int,7> survivors(int side) const;
    [[nodiscard]] std::array<int,7> wounds(int side) const;
    [[nodiscard]] std::int64_t experience_award(int side) const;
    [[nodiscard]] static std::int64_t damage(const BattleUnit& from, const BattleUnit& to);
private:
    explicit Battle(std::vector<BattleUnit> units) : units_{std::move(units)} {}
    void next();
    void finish_if_needed();
    std::vector<BattleUnit> units_;
    int active_ = -1, round_ = 1, movement_left_ = 0;
    std::optional<int> winner_;
    std::string message_;
};
}
