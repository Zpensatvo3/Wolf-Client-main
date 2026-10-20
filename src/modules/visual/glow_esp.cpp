#include "glow_esp.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "../../services/game/client_bridge.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../ui_redirect.hpp"
#include <vector>
#include <iostream>

// ---- Game thread: natively toggle glowing for players ----
void modules::glow_esp::run(::cache& cache)
{
    if constexpr (mc::target.build_id == 12111)
        if (!native_fallback_.load(std::memory_order_acquire)) return;
    jni::frame frame{};

    try
    {
        if (!cache.is_valid() || !cache.instance || !cache.world || !cache.player) return;

        JNIEnv* env = jni::get_env();

        auto iterable = platform::adapter::get().get_entities(cache.world);
        if (iterable) {
            auto iterator = iterable.iterator.call();
            if (iterator) {
                while (iterator.hasNext.call() == JNI_TRUE) {
                    auto obj = iterator.next.call();
                    if (obj) {
                        maps::Entity entity(obj.object_instance);
                        if (!entity) continue;
                        if (entity.is_same_object(cache.player)) continue;
                        
                        if (!entity.is_instance_of<maps::PlayerEntity>()) continue;
                        
                        if (entity.isAlive.call() == JNI_FALSE) continue;
                        const jboolean glow = services::anti_bot::is_bot(entity) ? JNI_FALSE : JNI_TRUE;

                        // Call native setGlowing function on the entity
                        try {
                            entity.setGlowing(glow);
                        } catch (...) {
                            if (env->ExceptionCheck()) env->ExceptionClear();
                        }
                        
                        try {
                            entity.setFlag(6, glow);
                        } catch (...) {
                            if (env->ExceptionCheck()) env->ExceptionClear();
                        }
                    }
                }
            }
        }
    } catch (...) {
        JNIEnv* env = jni::get_env();
        if (env && env->ExceptionCheck()) env->ExceptionClear();
    }
}

// ---- Disable module: remove native glowing from all players ----
void modules::glow_esp::on_disable(::cache& cache)
{
    if constexpr (mc::target.build_id == 12111) {
        reset_requested_.store(true, std::memory_order_release);
        if (!native_fallback_.load(std::memory_order_acquire)) return;
    }
    jni::frame frame{};

    try
    {
        if (!cache.is_valid() || !cache.instance || !cache.world || !cache.player) return;

        JNIEnv* env = jni::get_env();

        auto iterable = platform::adapter::get().get_entities(cache.world);
        if (iterable) {
            auto iterator = iterable.iterator.call();
            if (iterator) {
                while (iterator.hasNext.call() == JNI_TRUE) {
                    auto obj = iterator.next.call();
                    if (obj) {
                        maps::Entity entity(obj.object_instance);
                        if (!entity) continue;
                        
                        if (!entity.is_instance_of<maps::PlayerEntity>()) continue;

                        try {
                            entity.setGlowing(JNI_FALSE);
                        } catch (...) {
                            if (env->ExceptionCheck()) env->ExceptionClear();
                        }
                        try {
                            entity.setFlag(6, JNI_FALSE);
                        } catch (...) {
                            if (env->ExceptionCheck()) env->ExceptionClear();
                        }
                    }
                }
            }
        }
    } catch (...) {
        JNIEnv* env = jni::get_env();
        if (env && env->ExceptionCheck()) env->ExceptionClear();
    }
}

// We don't render 2D boxes anymore, so this is empty.
void modules::glow_esp::render_overlay()
{
    if constexpr (mc::target.build_id != 12111) return;
    if (native_fallback_.load(std::memory_order_acquire) || !platform::jni_context::bind_current_thread()) return;
    namespace bridge = services::game::client_bridge;
    try {
        jni::frame frame{80};
        if (reset_requested_.exchange(false, std::memory_order_acq_rel) || !enabled) {
            if (!bridge::release_glow()) { reset_requested_.store(true, std::memory_order_release); return; }
            if (!enabled) return;
        }
        bridge::glow_snapshot observed;
        const auto backend = bridge::capture_glow(observed);
        if (backend == services::game::hybrid::result::native_fallback) { native_fallback_.store(true, std::memory_order_release); return; }
        if (backend != services::game::hybrid::result::ready) { bridge::release_glow(); return; }
        std::vector<bool> allowed(observed.count, false);
        for (int i = 0; i < observed.count; ++i) {
            jni::frame candidate_frame{16};
            auto raw = jni::get_env()->GetObjectArrayElement(observed.players, i);
            if (platform::jni_context::clear_exception()) { bridge::release_glow(); return; }
            maps::Entity entity(raw, false);
            allowed[i] = entity && !services::anti_bot::is_bot(entity);
        }
        if (!bridge::apply_glow(observed, allowed)) bridge::release_glow();
    } catch (...) { platform::jni_context::clear_exception(); bridge::release_glow(); }
}

void modules::glow_esp::on_render_shutdown()
{
    services::game::client_bridge::release_glow();
}

// Options can be empty or have different settings if needed
void modules::glow_esp::render_options()
{
    gui::settings::render(settings_, module_settings::glow_esp::schema());
}
