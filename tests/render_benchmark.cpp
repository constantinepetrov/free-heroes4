#include "render_benchmark.hpp"
#include <algorithm>
#include <array>
#include <ctime>
#include <format>
#include <stdexcept>
#include <sys/resource.h>
#include "h4/app/application.hpp"
#include "h4/assets/assets.hpp"
#include "h4/core/performance.hpp"
#include "h4/cursor/cursor_source.hpp"
#include "h4/formats/bitmap_group.hpp"
#include "h4/formats/font.hpp"
#include "h4/formats/scenario/header.hpp"
#include "h4/formats/terrain_texture.hpp"
#include "h4/graphics/canvas.hpp"
#include "h4/model/map.hpp"
#include "h4/rules/battle.hpp"
#include "h4/screens/adventure/map_window.hpp"
#include "h4/screens/battle/battle_view.hpp"
#include "h4/screens/boot/main_menu_view.hpp"
namespace h4::tests {
namespace {
template<class T> T take(core::Result<T> result){if(!result) throw std::runtime_error(result.error().message());return std::move(*result);}
void check(bool b,const char* what){if(!b) throw std::runtime_error(what);}
struct Bytes {
 std::vector<std::byte> v;
 void u8(unsigned n){v.push_back(static_cast<std::byte>(n));}
 void u16(unsigned n){u8(n&255);u8(n>>8);}
 void u32(unsigned n){u16(n&65535);u16(n>>16);}
 void text(std::string_view s){u16(static_cast<unsigned>(s.size()));for(char c:s)u8(static_cast<unsigned char>(c));}
};
formats::BitmapGroup layers(){
 Bytes b;b.u16(7);
 const auto layer=[&](std::string_view name,int x,int y,int w,int h,int alpha){
  b.u16(4);b.u16(0);for(unsigned i=0;i<4;++i){b.u8(35+i*30);b.u8(75+i*40);b.u8(20+i*30);}
  b.text(name);b.u8(alpha?4:0);b.u32(static_cast<unsigned>(x));b.u32(static_cast<unsigned>(y));b.u32(static_cast<unsigned>(x+w));b.u32(static_cast<unsigned>(y+h));
  for(int j=0;j<h;++j){b.u16(0);b.u16(static_cast<unsigned>(w));b.u32(static_cast<unsigned>(j*w));}
  for(int j=0;j<w*h;++j)b.u8(static_cast<unsigned>(j%4));
  if(alpha)for(int j=0;j<(w*h+1)/2;++j)b.u8(static_cast<unsigned>((j%16)*17));
 };
 int y=170;for(auto n:{"New_Game_Released","Load_Game_Released","Options_Released","Quit_Released","Network_Released"}){layer(n,930,y,280,80,0);y+=160;}
 layer("opaque",0,0,64,80,0);layer("alpha",0,0,64,80,1);
 return take(formats::BitmapGroup::parse(b.v));
}
formats::TerrainTexture texture(){
 Bytes b;
 for(std::size_t t=0;t<formats::kTerrainTiles;++t){Bytes rows;std::vector<unsigned> offsets;
  for(auto width:formats::terrain_row_widths()){offsets.push_back(static_cast<unsigned>(rows.v.size()));rows.u8(0);rows.u8(width);for(unsigned i=0;i<width;++i)rows.u8(1+(i+static_cast<unsigned>(t))%3);}
  b.u32(static_cast<unsigned>(rows.v.size()));b.v.insert(b.v.end(),rows.v.begin(),rows.v.end());for(auto o:offsets)b.u32(o);
 }
 b.u32(256);for(unsigned i=0;i<256;++i){b.u8(i/3);b.u8(i);b.u8(i/2);}
 return take(formats::TerrainTexture::parse(b.v));
}
model::Map map_fixture(){
 Bytes b;b.u16(20);b.u16(128);b.u8(1);b.u32(42);b.u8(1);b.u8(0);b.u8(1);b.u8(0);b.u8(1);b.text("Procedural performance fixture");b.u8(1);b.text("");b.u16(0);b.u8(0);b.u8(0);b.u8(0);
 const auto header=take(formats::ScenarioHeader::parse(b.v));
 for(int i=0;i<12;++i){b.u16(0);b.u8(0);b.u16(0);}b.u8(0);b.u8(0);for(int i=0;i<4;++i)b.u8(0);b.u32(0);
 for(auto span:formats::tile_spans(128)) for(int y=span.start;y<span.stop;++y){b.u8(1);b.u8(0);b.u8(0);b.u8(0);b.u8(0);b.u16(0);b.u8(0);b.u8(0);}
 for(std::size_t i=0;i<formats::lattice_cells(formats::vertex_spans(128));++i)b.u8(0);
 return take(model::Map::parse(b.v,header,header.body_offset()));
}
class FixtureAssets final:public assets::Assets {
public:
 formats::BitmapGroup art=layers();formats::TerrainTexture ground=texture();formats::Font font=formats::Font::builtin();graphics::Surface565 background{{1280,1024},graphics::Color565{0x1963}};
 const formats::BitmapGroup* group(std::string_view n)const override{return n.starts_with("menu.main.")?&art:nullptr;}
 const formats::TerrainTexture* terrain(std::string_view)const override{return &ground;}
 const formats::Animation* animation(std::string_view)const override{return nullptr;}
 const formats::AdvObject* object(std::string_view)const override{return nullptr;}
 const formats::AdvActor* actor(std::string_view)const override{return nullptr;}
 const formats::ActorSequence* sequence(std::string_view)const override{return nullptr;}
 const formats::CombatActor* combat_actor(std::string_view)const override{return nullptr;}
 const formats::TransitionLibrary* transitions()const override{return nullptr;}
 const graphics::Surface565* painting(std::string_view)const override{return &background;}
 const formats::Font* font_for_size(std::int32_t)const override{return &font;}
 std::string_view text(std::string_view)const override{return {};}
 assets::InterfaceHelp help(std::string_view,std::string_view)const override{return {};}
 std::vector<std::byte> game_map(std::string_view)const override{return {};}
 core::Pending<assets::Prefetched> prefetch(std::span<const assets::AssetRequest>)const override{return {};}
 void adopt(assets::Prefetched)const override{}
};
class NullCursor final:public cursor::CursorSource {
 const platform::CursorImage* image(const cursor::CursorState&)override{return nullptr;}
 void advance(core::Instant)override{}
};
rules::Battle battle_fixture(){
 std::vector<rules::BattleUnit> units;
 for(int side=0;side<2;++side)for(int slot=0;slot<7;++slot){rules::BattleUnit u;u.side=side;u.slot=slot;u.at={side==0?1:10,slot+1};u.name="Procedural unit";u.initial_count=10;u.max_hp=20;u.hp=200;u.attack=10;u.defense=10;u.damage_low=2;u.damage_high=4;u.speed=5;u.movement=4;units.push_back(u);}
 return take(rules::Battle::start(std::move(units)));
}
class FixtureView final:public screens::View {
public:
 FixtureAssets assets;model::Map map=map_fixture();screens::MapWindow map_view{{{0,0},{960,1024}}};rules::Battle battle=battle_fixture();
 std::unique_ptr<screens::MainMenuView> menu=take(screens::MainMenuView::build(assets,{1280,1024},{}));
 screens::BattleView battle_view{{1280,1024},assets.font,battle,std::vector<const formats::Layer*>(14,assets.art.find("opaque"))};
 int mode=0,frame=0;
 FixtureView(){map_view.show_map(assets,map,nullptr);map_view.set_camera({1200,600});}
 void handle(const platform::InputEvent& e)override{if(mode==0)menu->handle(e);else if(mode==4)battle_view.handle(e);}
 void advance(core::Instant)override{++frame;if(mode==2)map_view.set_camera({1200+frame%64,600});}
 void paint(const graphics::Canvas& c)const override{
  if(mode==0){menu->paint(c);return;}
  if(mode==4){battle_view.paint(c);return;}
  map_view.paint(c);
  const auto* art=assets.art.find("alpha");
  for(int n=0;n<100;++n)c.draw_layer(*art,{(n*73)%880,(n*157)%900});
  c.fill_rect({{960,0},{320,1024}},{91,63,29,255});
  for(int n=0;n<20;++n)c.draw_text_line(assets.font,"Army HP 120 / 120",{980,20+n*40},{graphics::colors::white,graphics::colors::black});
  if(mode==2)c.draw_layer(*assets.art.find("opaque"),{300+frame%300,500});
  if(mode==3){c.fill_rect({{200,200},{700,460}},{194,170,115,255});for(int n=0;n<12;++n)c.draw_text_line(assets.font,"Procedural window text for renderer regression",{225,230+n*28},{graphics::colors::black,{}});}
 }
};
class FixtureController final:public app::Controller {
public:
 FixtureView fixture;
 screens::View& view()noexcept override{return fixture;}
 core::Size preferred_frame(core::Size)const override{return {1280,1024};}
};
}
std::string render_benchmark(platform::Window& window){
 core::perf::enable();NullCursor cursor;auto controller=std::make_unique<FixtureController>();auto* fixture=&controller->fixture;
 app::Application application{window,cursor,std::move(controller)};std::string result="[";
 for(int mode=0;mode<5;++mode){fixture->mode=mode;fixture->frame=0;fixture->map_view.set_camera({1200,600});
  for(int n=0;n<5;++n)check(application.step(std::chrono::milliseconds{16}),"Warmup exited");
  const auto cpu=std::clock();const auto wall=core::perf::Clock::now();std::vector<core::perf::Times> frames;
  for(int n=0;n<120;++n){check(application.step(std::chrono::milliseconds{16}),"Frame exited");frames.push_back(core::perf::current());}
  const double seconds=std::chrono::duration<double>(core::perf::Clock::now()-wall).count();const double cpu_s=static_cast<double>(std::clock()-cpu)/CLOCKS_PER_SEC;
  struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
  if(mode)result+=",";
  constexpr std::array<std::string_view,5> names{"main-menu","map-static","map-moving","window","battle"};
  auto stats=core::perf::summary(frames);
  result+=std::format("{{\"scenario\":\"{}\",\"size\":\"1280x1024\",\"cpu_percent\":{:.2f},\"peak_rss_kib\":{},\"timing\":{}}}",names[static_cast<std::size_t>(mode)],seconds>0?100*cpu_s/seconds:0,usage.ru_maxrss,stats);
 }
 return result+"]";
}
}
