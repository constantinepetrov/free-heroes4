#include <SDL3/SDL_main.h>
#ifdef __ANDROID__
#include <SDL3/SDL.h>
#endif

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#ifdef __ANDROID__
#include "../../tests/render_benchmark.hpp"
#endif
#include "h4/core/performance.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <print>
#include <string_view>
#include <vector>

#include "app_icon.hpp"
#include "first_boot.hpp"
#include "h4/app/application.hpp"
#include "h4/app/boot/main_menu_controller.hpp"
#include "h4/app/installation_choice.hpp"
#include "h4/app/launch_options.hpp"
#include "h4/assets/asset_library.hpp"
#include "h4/cursor/cursor_library.hpp"
#include "h4/io/archive_chain.hpp"
#include "h4/model/game_database.hpp"
#include "h4/model/installation.hpp"
#include "h4/model/user_settings.hpp"
#include "h4/platform/sdl3/sdl_paths.hpp"
#include "h4/platform/sdl3/sdl_window.hpp"
#include "h4/screens/boot/main_menu_view.hpp"
#include "h4/screens/boot/setup_view.hpp"
#include "h4/screens/shell/help_overlay.hpp"

namespace {

int report(std::string_view stage, const h4::core::Error& error) {
    std::println(stderr, "openh4: {}: {}", stage, error.message());
#ifdef __ANDROID__
    const std::string message = std::string{stage} + ": " + error.message();
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenH4", message.c_str(), nullptr);
#endif
    return EXIT_FAILURE;
}

}

int main(int argc, char** argv) {
    using namespace h4;
#ifdef __ANDROID__
    core::perf::enable([](std::string_view s){SDL_Log("openh4.perf %s",std::string{s}.c_str());});
#endif

#ifdef __ANDROID__
    if(argc==3 && std::string_view{argv[1]}=="--render-benchmark") {
        try {
            auto window=platform::sdl3::create_window("OpenH4 renderer benchmark",{1280,1024},{});
            if(!window) throw std::runtime_error(window.error().message());
            const auto json=tests::render_benchmark(**window);
            std::ofstream out{argv[2]};out<<json;out.close();
            SDL_Log("openh4.benchmark completed");return 0;
        } catch(const std::exception& e){std::ofstream out{argv[2]};out<<"{\"error\":\"benchmark failed; see SDL log\"}";SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"benchmark: %s",e.what());return 1;}
    }
#endif
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    const char* env    = std::getenv("H4_DATA_DIR");
    const auto options = app::LaunchOptions::parse(args, env);
    if (!options) {
        std::print(stderr, "openh4: {}\n{}", options.error().message(), app::LaunchOptions::usage());
        return 2;
    }
    if (options->show_help) {
        std::print("{}", app::LaunchOptions::usage());
        return EXIT_SUCCESS;
    }

    const auto home = platform::sdl3::preferences_path("openh4");
    if (!home)
        return report("finding the settings directory", home.error());
    const auto settings_file = *home / model::kSettingsFile;
    auto loaded              = root::load_settings(settings_file);
    if (!loaded)
        return report("reading the settings", loaded.error());
    model::UserSettings& settings = loaded->settings;
    bool changed                  = !loaded->found;

    std::unique_ptr<platform::Window> window;
    auto data_dir = app::choose_installation(*options, settings, root::holds_installation);
    if (!data_dir) {
        auto opened = platform::sdl3::create_window("openh4", screens::SetupView::kFrame, root::app_icon_png());
        if (!opened)
            return report("opening the window", opened.error());
        window   = std::move(*opened);
        data_dir = root::run_setup(*window, *home);
        if (!data_dir)
            return EXIT_SUCCESS;
        settings.data_dir = *data_dir;
        changed           = true;
    }
    if (!loaded->found)
        root::import_retail_settings(settings, *data_dir);
    if (changed)
        root::save_settings(settings_file, settings);

    const screens::Resolution resolution = options->resolution
                                               .or_else([&] {
                                                   return screens::resolution_for_width(settings.game.resolution);
                                               })
                                               .value_or(screens::Resolution::r1024x768);

    const auto installation = io::ArchiveChain::mount_installation(*data_dir);
    if (!installation)
        return report("mounting the installation", installation.error());
    const model::InstalledContent content = model::InstalledContent::detect(*installation);

    const auto database = model::GameDatabase::load(*installation);
    if (!database)
        return report("loading the rules data", database.error());
    std::println("openh4: {} archives mounted, {} creature types",
                 installation->archives().size(),
                 database->creatures().size());

    const assets::AssetLibrary assets{*installation};
    cursor::CursorLibrary cursors{assets};

    auto view = screens::MainMenuView::build(assets, screens::size_of(resolution), screens::MainMenuModel{});
    if (!view)
        return report("building the main menu", view.error());

    if (!window) {
        auto opened = platform::sdl3::create_window("openh4", screens::size_of(resolution), root::app_icon_png());
        if (!opened)
            return report("opening the window", opened.error());
        window = std::move(*opened);
    }

    auto controller = std::make_unique<app::MainMenuController>(
        std::move(*view),
        assets,
        content,
        app::maps_dir_of(*data_dir),
        [](std::string_view message) {
#ifdef __ANDROID__
            SDL_Log("openh4: %s", std::string{message}.c_str());
#else
            std::println("openh4: {}", message);
#endif
        },
        &*database
    );
    controller->set_settings(settings.game);
    controller->set_settings_saver([&](const model::GameSettings& game) {
        settings.game = game;
        root::save_settings(settings_file, settings);
    });

    auto help = screens::HelpOverlay::build(assets);
    if (!help)
        return report("building the help", help.error());
    app::Application application{*window, cursors, std::move(controller), std::move(*help)};
    application.run();
    return EXIT_SUCCESS;
}
