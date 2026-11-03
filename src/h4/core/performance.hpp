#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
namespace h4::core::perf {
enum Stage : std::size_t { input, update, paint, map, terrain, objects, shroud, upload, present, controller, total, count };
using Times = std::array<double,count>;
using Clock = std::chrono::steady_clock;
void enable(std::function<void(std::string_view)> sink = {});
bool enabled() noexcept;
std::int64_t peak_rss_kib() noexcept;
void begin_frame();
void end_frame();
void add(Stage stage,double ms) noexcept;
Times current() noexcept;
std::string summary(std::span<const Times> frames);
class Scope {
public:
    explicit Scope(Stage stage):stage_{stage},active_{enabled()} {if(active_) start_=Clock::now();}
    ~Scope(){if(active_) add(stage_,std::chrono::duration<double,std::milli>(Clock::now()-start_).count());}
private:
    Stage stage_; bool active_; Clock::time_point start_;
};
}
