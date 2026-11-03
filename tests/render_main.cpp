#include <iostream>
#include <stdexcept>
#include "render_benchmark.hpp"
#include "h4/platform/window.hpp"
#include "h4/graphics/surface565.hpp"
class MemoryWindow final:public h4::platform::Window {
public:
 int frames=0;
 h4::core::Size logical_size()const noexcept override{return {1280,1024};}
 h4::core::Size window_size()const noexcept override{return {1280,1024};}
 void set_logical_size(h4::core::Size)override{}
 std::vector<h4::platform::InputEvent> poll_events()override{return {};}
 void present(const h4::graphics::Surface565& f)override{if(f.size()!=logical_size())throw std::runtime_error("wrong frame size");++frames;}
 void set_cursor(const h4::platform::CursorImage*)override{}
 void open_file_dialog(std::string_view,std::string_view)override{}
 bool offers_web_sign_in()const noexcept override{return false;}
 void open_web_sign_in(std::string_view,std::string_view)override{}
};
int main(){try{MemoryWindow w;std::cout<<h4::tests::render_benchmark(w)<<'\n';if(w.frames!=930)throw std::runtime_error("Missing rendered frames");return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
