#include <jni.h>
#include <exception>
#include <filesystem>
#include <string>
#include "h4/io/archive.hpp"
#include "h4/io/archive_chain.hpp"
#include "h4/model/game_database.hpp"
#include "h4/assets/asset_library.hpp"
#include "h4/screens/boot/main_menu_view.hpp"

extern "C" JNIEXPORT jstring JNICALL
Java_org_openh4_android_LauncherActivity_validateResources(JNIEnv* env, jclass, jstring path) {
    if (!path) return env->NewStringUTF("Missing resource path");
    const char* raw = env->GetStringUTFChars(path, nullptr);
    if (!raw) return nullptr; // Pending JVM out-of-memory exception.
    const std::filesystem::path data{raw};
    env->ReleaseStringUTFChars(path, raw);
    try {
        // The upstream chain silently skips broken optional archives; import must reject them.
        for (const auto& file : std::filesystem::directory_iterator(data)) {
            if (file.path().extension() != ".h4r") continue;
            auto archive = h4::io::Archive::open(file.path());
            if (!archive) return env->NewStringUTF(archive.error().message().c_str());
        }
        auto chain = h4::io::ArchiveChain::mount_installation(data);
        if (!chain) return env->NewStringUTF(chain.error().message().c_str());
        auto database = h4::model::GameDatabase::load(*chain);
        if (!database) return env->NewStringUTF(database.error().message().c_str());
        h4::assets::AssetLibrary assets{*chain};
        auto menu = h4::screens::MainMenuView::build(assets, {1024, 768}, {});
        if (!menu) return env->NewStringUTF(menu.error().message().c_str());
        return nullptr;
    } catch (const std::exception& e) {
        return env->NewStringUTF(e.what());
    }
}

#include "../../tests/battle_cases.hpp"
extern "C" JNIEXPORT jstring JNICALL Java_org_openh4_android_LauncherActivity_combatSelfTest(JNIEnv* env,jclass) {
    try {h4::tests::battle_checks();return nullptr;}
    catch(const std::exception& e) {return env->NewStringUTF(e.what());}
}
