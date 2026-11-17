#include "glow_esp.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "../../services/game/client_bridge.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../ui_redirect.hpp"
#include <vector>
#include <algorithm>
#include <iostream>

namespace {
    struct glow_record { jobject entity=nullptr; jboolean original=JNI_FALSE, written=JNI_TRUE; };
    std::vector<glow_record> glow_records;
    jobject glow_world=nullptr;
    void release_native_glow(bool restore,bool final)
    {
        auto* env=jni::get_env(); if (!env) return;
        jni::frame frame{};
        for (auto it=glow_records.begin();it!=glow_records.end();) {
            bool done=!restore;
            if (restore) {
                try {
                    maps::Entity entity(it->entity,false);
                    auto current=entity.getFlag(6);
                    if (!platform::jni_context::clear_exception()) {
                        // Preserve a newer, distinguishable external update.
                        if (current==it->written) entity.setFlag(6,it->original);
                        done=!platform::jni_context::clear_exception();
                    }
                } catch (...) { platform::jni_context::clear_exception(); }
            }
            if (done || final) { env->DeleteGlobalRef(it->entity); it=glow_records.erase(it); }
            else ++it;
        }
        if (glow_records.empty() && glow_world) { env->DeleteGlobalRef(glow_world); glow_world=nullptr; }
    }
}

void modules::glow_esp::run(::cache& cache)
{
    if constexpr (mc::target.build_id==12111)
        if (!native_fallback_.load(std::memory_order_acquire)) return;
    auto* env=jni::get_env();
    if (!env || !cache.is_valid() || !cache.world || !cache.player) return;
    jni::frame frame{};
    try {
        if (glow_world && !env->IsSameObject(glow_world,cache.world.object_instance))
            release_native_glow(false,true);
        if (!glow_world) glow_world=env->NewGlobalRef(cache.world.object_instance);
        if (!glow_world || platform::jni_context::clear_exception()) return;
        // Retain undo records only for currently eligible players.
        std::vector<jobject> seen;
        auto iterable=platform::adapter::get().get_players(cache.world);
        if (!iterable) return;
        maps::Iterable collection(iterable.object_instance,false);
        auto iterator=collection.iterator.call();
        if (!iterator || platform::jni_context::clear_exception()) return;
        while (iterator.hasNext.call()==JNI_TRUE) {
            auto object=iterator.next.call();
            maps::Entity entity(object.object_instance,false);
            if (!entity || entity.is_same_object(cache.player) ||
                !entity.is_instance_of<maps::PlayerEntity>() || entity.isAlive.call()==JNI_FALSE ||
                services::anti_bot::is_bot(entity)) continue;
            auto found=std::find_if(glow_records.begin(),glow_records.end(),[&](const auto& record) {
                return env->IsSameObject(record.entity,entity.object_instance)==JNI_TRUE;
            });
            if (found!=glow_records.end()) { seen.push_back(found->entity); continue; }
            const auto original=entity.getFlag(6);
            if (platform::jni_context::clear_exception() || original==JNI_TRUE) continue;
            auto owned=env->NewGlobalRef(entity.object_instance);
            if (!owned || platform::jni_context::clear_exception()) continue;
            // Record before mutation so a failed write still has an undo owner.
            glow_records.push_back({owned,original,JNI_TRUE}); seen.push_back(owned);
            entity.setFlag(6,JNI_TRUE);
            platform::jni_context::clear_exception();
        }
        for (auto it=glow_records.begin();it!=glow_records.end();) {
            if (std::find(seen.begin(),seen.end(),it->entity)!=seen.end()) { ++it; continue; }
            maps::Entity entity(it->entity,false);
            auto current=entity.getFlag(6);
            if (platform::jni_context::clear_exception()) { ++it; continue; }
            if (current==it->written) entity.setFlag(6,it->original);
            if (platform::jni_context::clear_exception()) { ++it; continue; }
            env->DeleteGlobalRef(it->entity); it=glow_records.erase(it);
        }
    } catch (...) { platform::jni_context::clear_exception(); }
}

void modules::glow_esp::on_disable(::cache&)
{
    reset_requested_.store(true,std::memory_order_release);
    release_native_glow(true,false);
}
void modules::glow_esp::maintenance(::cache&) { if (!enabled) release_native_glow(true,false); }
void modules::glow_esp::on_worker_shutdown(::cache&) { release_native_glow(true,true); }

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
