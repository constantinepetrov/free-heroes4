#include "h4/core/performance.hpp"
#include <algorithm>
#include <format>
#include <numeric>
#include <vector>
namespace h4::core::perf {
namespace {
thread_local bool on=false;
thread_local Times times{};
thread_local Clock::time_point start;
thread_local Clock::time_point last_report;
thread_local std::vector<Times> batch;
thread_local std::function<void(std::string_view)> report;
}
void enable(std::function<void(std::string_view)> sink) {on=true;report=std::move(sink);batch.clear();last_report=Clock::now();}
bool enabled() noexcept {return on;}
void begin_frame(){if(on){times={};start=Clock::now();}}
void add(Stage stage,double ms) noexcept {times[stage]+=ms;}
Times current() noexcept {return times;}
std::string summary(std::span<const Times> frames) {
 if(frames.empty()) return "{}";
 Times mean{};std::vector<double> samples; samples.reserve(frames.size());
 for(const auto& f:frames){for(std::size_t i=0;i<count;++i) mean[i]+=f[i];samples.push_back(f[total]);}
 for(auto& v:mean) v/=static_cast<double>(frames.size());
 std::ranges::sort(samples);
 const auto q=[&](std::size_t p){return samples[std::min(samples.size()-1,(samples.size()*p+99)/100-1)];};
 const double ui=std::max(0.0,mean[paint]-mean[map]);
 return std::format("{{\"frames\":{},\"fps\":{:.2f},\"median_ms\":{:.3f},\"p95_ms\":{:.3f},\"p99_ms\":{:.3f},\"input_ms\":{:.3f},\"update_ms\":{:.3f},\"paint_ms\":{:.3f},\"map_ms\":{:.3f},\"terrain_ms\":{:.3f},\"objects_ms\":{:.3f},\"shroud_ms\":{:.3f},\"ui_ms\":{:.3f},\"upload_ms\":{:.3f},\"present_ms\":{:.3f}}}",frames.size(),mean[total]>0?1000.0/mean[total]:0,q(50),q(95),q(99),mean[input],mean[update],mean[paint],mean[map],mean[terrain],mean[objects],mean[shroud],ui,mean[upload],mean[present]);
}
void end_frame(){
 if(!on) return;
 times[total]=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
 if(!report) return;
 batch.push_back(times);
 if(Clock::now()-last_report>=std::chrono::seconds{5} || batch.size()>=600){report(summary(batch));batch.clear();last_report=Clock::now();}
}
}
