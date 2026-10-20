#include "sky.hpp"
#include "sky_bindings.hpp"
#include "sky_panorama.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/ui_state.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/overlay_context.hpp"
#include "../../ui/settings_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
#if ONESTEP_MINECRAFT_VERSION < 12111
    struct saved_color { int original=0,last=0; bool owned=false; };
    struct saved_effects
    {
        sky_bindings::Effects object{nullptr,true};
        saved_color sky,fog;
    };
#endif
    int blend_rgb(int original,ImVec4 target,float strength)
    {
        const auto channel=[&](int shift,float value) {
            value=std::isfinite(value) ? std::clamp(value,0.f,1.f) : 0.f;
            return std::clamp(static_cast<int>(std::lround(((original>>shift)&255)*(1-strength)+value*255*strength)),0,255);
        };
        return channel(16,target.x)<<16 | channel(8,target.y)<<8 | channel(0,target.z);
    }
#if ONESTEP_MINECRAFT_VERSION < 12111
    bool restore(auto& field,saved_color& saved)
    {
        if (!saved.owned) return true;
        const int current=field.get();
        if (platform::jni_context::clear_exception()) return false;
        if (current==saved.last)
        {
            field.set(static_cast<jint>(saved.original));
            if (platform::jni_context::clear_exception()) return false;
        }
        saved.owned=false;
        return true;
    }
    bool apply(auto& field,saved_color& saved,ImVec4 color,float strength)
    {
        const int current=field.get();
        if (platform::jni_context::clear_exception()) return false;
        // A different writer becomes the new baseline; never restore over it.
        if (!saved.owned || current!=saved.last) saved.original=current;
        const int next=blend_rgb(saved.original,color,strength);
        if (next!=current)
        {
            field.set(static_cast<jint>(next));
            if (platform::jni_context::clear_exception()) return false;
        }
        saved.last=next; saved.owned=true;
        return true;
    }
#endif
}

struct modules::sky::runtime
{
    services::game::overlay_session session;
    sky_panorama::renderer panorama;
#if ONESTEP_MINECRAFT_VERSION < 12111
    std::vector<saved_effects> effects;
#endif
    unsigned generation=0;
    double next_update=0,next_capture=0;

    bool reset()
    {
        bool complete=true;
#if ONESTEP_MINECRAFT_VERSION < 12111
        for (auto& entry:effects)
        {
            if (!restore(entry.object.skyColor,entry.sky)) complete=false;
            if (!restore(entry.object.fogColor,entry.fog)) complete=false;
        }
        if (complete) effects.clear();
#endif
        if (complete) { session.reset(); next_update=next_capture=0; }
        return complete;
    }
#if ONESTEP_MINECRAFT_VERSION < 12111
    bool capture(const services::game::client_context& context)
    {
        using platform::jni_context::clear_exception;
        sky_bindings::World world(context.world.object_instance,false);
        if (!world.getRegistryManager.is_valid() || clear_exception()) return false;
        auto manager=world.getRegistryManager.call();
        if (clear_exception() || !manager || !manager.get.is_valid()) return false;
        auto key=sky_bindings::Keys{}.BIOME.get();
        if (clear_exception() || !key) return false;
        auto registry=manager.get.call(key);
        if (clear_exception() || !registry) return false;
        auto iterator=registry.iterator.call();
        if (clear_exception() || !iterator) return false;
        std::vector<saved_effects> candidate;
        for (int i=0;i<2048;++i)
        {
            const bool more=iterator.hasNext.call()==JNI_TRUE;
            if (clear_exception()) return false;
            if (!more)
            {
                if (candidate.empty()) return false;
                effects=std::move(candidate);
                return true;
            }
            jni::frame item_frame{16};
            auto item=iterator.next.call();
            if (clear_exception() || !item) return false;
            sky_bindings::Biome biome(item.object_instance,false);
            auto effect=biome.getEffects.call();
            if (clear_exception() || !effect || !effect.skyColor.is_valid() || !effect.fogColor.is_valid()) return false;
            bool duplicate=false;
            for (const auto& previous:candidate)
                if (previous.object.is_same_object(effect)) { duplicate=true; break; }
            if (clear_exception()) return false;
            if (duplicate) continue;
            saved_effects entry;
            entry.object=sky_bindings::Effects(effect.object_instance,true);
            entry.sky.original=effect.skyColor.get();
            if (clear_exception()) return false;
            entry.fog.original=effect.fogColor.get();
            if (clear_exception()) return false;
            candidate.push_back(std::move(entry));
        }
        return false; // Refuse an unbounded/modded registry without changing it.
    }
#endif
};

modules::sky::sky() : module("sky","Visual","sky"),state_(std::make_unique<runtime>()) {}
modules::sky::~sky() = default;

void modules::sky::on_render_shutdown()
{
    state_->panorama.shutdown();
    if (!platform::jni_context::bind_current_thread()) return;
    try { jni::frame frame{32}; state_->reset(); }
    catch (...) { platform::jni_context::clear_exception(); }
}

void modules::sky::render_overlay()
{
    if (!platform::jni_context::bind_current_thread()) return;
    struct cleanup { ~cleanup() { platform::jni_context::clear_exception(); } } clear_on_exit;
    jni::frame frame{64};
    auto& state=*state_;
    const auto config=settings_.read();
    const bool reduced=gui::ui_state::get_preferences().reduce_motion;
    bool panorama_ready=false,colors_ready=false;
    int sky_rgb=0,fog_rgb=0;
    struct publish_panorama
    {
        sky_panorama::renderer& renderer;
        const module_settings::sky& config;
        const bool& ready;
        const bool& colors_ready;
        const int& sky_rgb;
        const int& fog_rgb;
        bool reduced;
        ~publish_panorama()
        {
            renderer.configure(config,ready,reduced);
            renderer.configure_colors(sky_rgb,fog_rgb,std::clamp(config.strength,0.f,1.f),config.match_fog,colors_ready);
        }
    } publish{state.panorama,config,panorama_ready,colors_ready,sky_rgb,fog_rgb,reduced};
    try
    {
        const auto generation=reset_generation_.load(std::memory_order_acquire);
        if (generation!=state.generation)
        {
            if (!state.reset()) return;
            state.generation=generation;
        }
        if (!enabled || gui::loader_visible || gui::should_unload) { state.reset(); return; }
        services::game::client_context context;
        if (!services::game::capture_client_context(context,true,false)) { state.reset(); return; }
        // The same biome registry is reused across dimensions and camera media.
        // Restore our air/sky colors before vanilla draws water, lava or a skyless world.
        sky_bindings::World world(context.world.object_instance,false);
        auto dimension=world.getDimension.call();
        if (platform::jni_context::clear_exception() || !dimension) { state.reset(); return; }
        const bool skylight=dimension.hasSkyLight.call()==JNI_TRUE;
        if (platform::jni_context::clear_exception() || !skylight) { state.reset(); return; }
        auto renderer=context.instance.gameRenderer.get();
        if (platform::jni_context::clear_exception() || !renderer) { state.reset(); return; }
        auto camera=renderer.getCamera.call();
        if (platform::jni_context::clear_exception() || !camera) { state.reset(); return; }
        sky_bindings::Camera view(camera.object_instance,false);
        auto medium=view.getSubmersionType.call();
        if (platform::jni_context::clear_exception() || !medium) { state.reset(); return; }
        auto air=sky_bindings::Submersion{}.NONE.get();
        if (platform::jni_context::clear_exception() || !air) { state.reset(); return; }
        const bool clear_air=air.is_same_object(medium);
        if (platform::jni_context::clear_exception() || !clear_air) { state.reset(); return; }
        const services::game::overlay_identity identity(context.world,context.player);
        if (!state.session.matches(identity))
        {
            if (!state.reset()) return;
            state.session.observe(identity);
        }
        if (!std::isfinite(config.strength) || config.strength<=0) { state.reset(); return; }
#if ONESTEP_MINECRAFT_VERSION >= 12111
        colors_ready=state.panorama.connect();
        panorama_ready=config.mode==1 && colors_ready;
#else
        panorama_ready=config.mode==1 && state.panorama.connect();
        const double now=ImGui::GetTime();
        if (state.effects.empty())
        {
            if (now<state.next_capture) return;
            state.next_capture=now+1.;
            if (!state.capture(context)) return;
        }
        if (now<state.next_update) return;
        state.next_update=now+1./15.;
#endif
        ImVec4 color;
        if (config.mode==1)
        {
            switch (config.panorama)
            {
            case 1: color={.14f,.13f,.20f,1.f}; break;
            case 2: color={.74f,.56f,.43f,1.f}; break;
            case 3: color={.82f,.43f,.65f,1.f}; break;
            case 4: color={.31f,.20f,.55f,1.f}; break;
            case 5: color={.32f,.40f,.66f,1.f}; break;
            default: color={.08f,.12f,.22f,1.f}; break;
            }
        }
        else switch (config.style)
        {
        case 1: color={1.f,.43f,.32f,1.f}; break;
        case 2: color={.46f,.24f,.78f,1.f}; break;
        case 3: color={.29f,.76f,1.f,1.f}; break;
        case 4: color={config.color[0],config.color[1],config.color[2],1.f}; break;
        default: color=gui::get_dynamic_color().Value; break;
        }
        auto fog=color;
        if (config.mode!=1 || config.panorama<3)
        {
            fog.x=fog.x*.67f+.28f; fog.y=fog.y*.67f+.28f; fog.z=fog.z*.67f+.28f;
        }
        const float strength=std::clamp(config.strength,0.f,1.f);
#if ONESTEP_MINECRAFT_VERSION >= 12111
        sky_rgb=blend_rgb(0,color,1.f);
        fog_rgb=blend_rgb(0,fog,1.f);
#else
        for (auto& entry:state.effects)
        {
            if (!apply(entry.object.skyColor,entry.sky,color,strength)) { state.reset(); return; }
            const bool ok=config.match_fog ? apply(entry.object.fogColor,entry.fog,fog,strength*.75f)
                : restore(entry.object.fogColor,entry.fog);
            if (!ok) { state.reset(); return; }
        }
#endif
    }
    catch (...) { platform::jni_context::clear_exception(); panorama_ready=colors_ready=false; state.reset(); }
}

void modules::sky::render_options()
{
    gui::settings::render(settings_,module_settings::sky::schema());
    if (settings_.read().mode==1)
    {
        const char* error=state_->panorama.error();
        if (error && *error) ImGui::TextWrapped("%s",error);
    }
}
