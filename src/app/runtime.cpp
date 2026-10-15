#include "runtime.hpp"
#include "module_loop.hpp"
#include "../auth/auth.hpp"
#include "../cache/cache.hpp"
#include "../configs/configs.hpp"
#include "../gui/gui.hpp"
#include "../loader/loader_ui.hpp"
#include "../modules/module_lifecycle.hpp"
#include "../platform/adapter/game_adapter.hpp"
#include "../platform/jni/jvm_session.hpp"
#include "../utils/version_manager.hpp"
#include <Windows.h>
#include <chrono>
#include <exception>
#include <string>
#include <thread>

namespace
{
    constexpr auto worker_interval = std::chrono::milliseconds(5);
    constexpr char config_endpoint[] = "wolf-auth-api.gogogamesas.workers.dev";

    // Declared after the game cache, so renderer/modules stop before its JNI refs die.
    struct runtime_services
    {
        cache& game;
        bool modules_started = false;
        bool renderer_started = false;
        bool configs_started = false;

        ~runtime_services()
        {
            gui::should_unload.store(true, std::memory_order_release);
            if (configs_started) configs::shutdown();
            auth::controlled_shutdown();
            loader_ui::closing();
            gui::draw = false;
            if (renderer_started) gui::shutdown();
            if (modules_started) modules::shutdown(game);
        }
    };
}

void app::run()
{
    loader_ui::initialize();
    try
    {
        auth::configure_config_service(config_endpoint);
        loader_ui::stage(0, "Locating Minecraft JVM runtime");
        while (!GetModuleHandleW(L"jvm.dll"))
        {
            if (gui::should_unload || (GetAsyncKeyState(VK_END) & 0x8000)) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        loader_ui::stage(1, "Attaching native JVM session");
        platform::jvm_session session;
        if (!session.open()) { loader_ui::fail(session.error().c_str()); return; }

        loader_ui::stage(2, "Resolving Minecraft runtime and mappings");
        mc::detect(session.env(), session.classes());
        platform::adapter::initialize(mc::get_version());
        const std::string version = std::string("Detected runtime: Fabric ") + mc::get_version_name();
        loader_ui::stage(2, version.c_str());

        cache game;
        runtime_services services{game};
        // Register before enabling render callbacks: the module list stays fixed
        // until gui::shutdown has joined the render side.
        loader_ui::stage(3, "Registering client modules");
        services.modules_started = true;
        modules::init();

        loader_ui::stage(4, "Installing OpenGL render hook");
        if (!gui::init()) { loader_ui::fail("OpenGL renderer hook could not be installed"); return; }
        services.renderer_started = true;
        services.configs_started = true;
        configs::initialize();
        loader_ui::ready();

        module_loop loop;
        while (!(GetAsyncKeyState(VK_END) & 0x8000) && !gui::should_unload)
        {
            loop.tick(game);
            std::this_thread::sleep_for(worker_interval);
        }
    }
    catch (const std::exception& error) { loader_ui::fail(error.what()); }
    catch (...) { loader_ui::fail("Onestep runtime initialization or execution failed"); }
}
