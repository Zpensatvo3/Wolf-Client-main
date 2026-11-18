#include "glow_esp.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "../../services/game/client_bridge.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../ui_redirect.hpp"
#include <vector>
#include <map>
#include <cstdint>
#include <algorithm>
#include <iostream>

namespace {
    struct glow_record {
        jobject entity=nullptr;
        jboolean original=JNI_FALSE, written=JNI_TRUE;
        bool applied=false;
        std::uint64_t seen=0;
    };
    // IDs are scoped by glow_world; Java identity also guards ID reuse.
    std::map<int,glow_record> glow_records;
    jobject glow_world=nullptr;
    std::uint64_t glow_generation=0;
    bool restore_native_glow(glow_record& record,bool restore)
    {
        if (!restore || !record.applied) return true;
        try {
            maps::Entity entity(record.entity,false);
            auto current=entity.getFlag(6);
            if (platform::jni_context::clear_exception()) return false;
            if (current==record.written) entity.setFlag(6,record.original);
            return !platform::jni_context::clear_exception();
        } catch (...) { platform::jni_context::clear_exception(); return false; }
    }
    void release_native_glow(bool restore,bool final)
    {
        auto* env=jni::get_env(); if (!env) return;
        jni::frame frame{};
        for (auto it=glow_records.begin();it!=glow_records.end();) {
            if (restore_native_glow(it->second,restore) || final) {
                env->DeleteGlobalRef(it->second.entity); it=glow_records.erase(it);
            } else ++it;
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
        auto iterable=platform::adapter::get().get_players(cache.world);
        if (!iterable) return;
        maps::Iterable collection(iterable.object_instance,false);
        auto iterator=collection.iterator.call();
        if (!iterator || platform::jni_context::clear_exception()) return;
        if (++glow_generation==0) { // Avoid a generation collision after wrap.
            for (auto& [id,record]:glow_records) record.seen=0;
            ++glow_generation;
        }
        while (iterator.hasNext.call()==JNI_TRUE) {
            auto object=iterator.next.call();
            if (platform::jni_context::clear_exception()) return; // Incomplete scan cannot retire owners.
            maps::Entity entity(object.object_instance,false);
            if (!entity || entity.is_same_object(cache.player) ||
                !entity.is_instance_of<maps::PlayerEntity>() || entity.isAlive.call()==JNI_FALSE ||
                services::anti_bot::is_bot(entity)) continue;
            const int id=entity.getId.call();
            if (platform::jni_context::clear_exception()) return;
            auto found=glow_records.find(id);
            if (found!=glow_records.end() && !env->IsSameObject(found->second.entity,entity.object_instance)) {
                // Reused numeric ID: finish the old object's undo before ownership changes.
                if (!restore_native_glow(found->second,true)) { found->second.seen=glow_generation; continue; }
                env->DeleteGlobalRef(found->second.entity); glow_records.erase(found); found=glow_records.end();
            }
            if (found==glow_records.end()) {
                const auto original=entity.getFlag(6);
                if (platform::jni_context::clear_exception() || original==JNI_TRUE) continue;
                auto owned=env->NewGlobalRef(entity.object_instance);
                if (!owned || platform::jni_context::clear_exception()) continue;
                try { found=glow_records.emplace(id,glow_record{owned,original,JNI_TRUE,false,0}).first; }
                catch (...) { env->DeleteGlobalRef(owned); throw; }
            }
            auto& record=found->second; record.seen=glow_generation;
            if (record.applied) continue;
            // A failed first write is retried. If someone else enabled glow
            // before retry, leave that value alone and abandon our pending owner.
            if (entity.getFlag(6)==JNI_TRUE) {
                if (platform::jni_context::clear_exception()) continue;
                env->DeleteGlobalRef(record.entity); glow_records.erase(found); continue;
            }
            if (platform::jni_context::clear_exception()) continue;
            entity.setFlag(6,JNI_TRUE);
            record.applied=!platform::jni_context::clear_exception();
        }
        if (platform::jni_context::clear_exception()) return;
        for (auto it=glow_records.begin();it!=glow_records.end();) {
            if (it->second.seen==glow_generation || !restore_native_glow(it->second,true)) { ++it; continue; }
            env->DeleteGlobalRef(it->second.entity); it=glow_records.erase(it);
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
