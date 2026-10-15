#include "glow_esp.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../ui_redirect.hpp"
#include <vector>
#include <iostream>

// ---- Game thread: natively toggle glowing for players ----
void modules::glow_esp::run(::cache& cache)
{
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

                        // Call native setGlowing function on the entity
                        try {
                            entity.setGlowing(JNI_TRUE);
                        } catch (...) {
                            if (env->ExceptionCheck()) env->ExceptionClear();
                        }
                        
                        try {
                            entity.setFlag(6, JNI_TRUE);
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
}

// Options can be empty or have different settings if needed
void modules::glow_esp::render_options()
{
    gui::settings::render(settings_, module_settings::glow_esp::schema());
}
