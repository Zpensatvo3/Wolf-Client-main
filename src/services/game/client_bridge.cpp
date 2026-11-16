#include "client_bridge.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../input/input_service.hpp"
#include "../../sdk/minecraft/common/client.hpp"
#include "../../../resource.h"
#include <Windows.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <cstdio>

namespace
{
    jobject bridge = nullptr;
    jmethodID prepare_id = nullptr, walk_id = nullptr, apply_walk_id = nullptr, inputs_id = nullptr, release_walk_id = nullptr;
    jmethodID glow_id = nullptr, apply_glow_id = nullptr, release_glow_id = nullptr, stop_id = nullptr;
    jmethodID set_inventory_busy_id = nullptr, prepare_walk_action_id = nullptr;
    std::atomic_bool inventory_busy_state{false};
    bool failed = false;
    std::array<bool, 3> prepared{}, rejected{}, suspended{};
    bool permitted(unsigned feature) {
        if (gui::loader_visible || gui::should_unload) return false;
        if (feature == 2) return true; // Visual: keep the overlay while menus/other windows have focus.
        const HWND window = gui::get_window();
        return !gui::draw && window && GetForegroundWindow() == window;
    }
    template<class T> T checked(JNIEnv* env, T value) {
        if (!value || env->ExceptionCheck()) throw std::runtime_error("Client JNI binding"); return value;
    }
    bool connect(JNIEnv* env) {
        if (failed) return false;
        if (bridge) return true;
        try {
            auto game = checked(env, jni::get_cached_jclass<maps::MinecraftClient>());
            auto klass = checked(env, env->FindClass("java/lang/Class"));
            auto get_loader = checked(env, env->GetMethodID(klass, "getClassLoader", "()Ljava/lang/ClassLoader;"));
            auto parent = checked(env, env->CallObjectMethod(game, get_loader));
            auto loader_type = checked(env, env->FindClass("java/net/URLClassLoader"));
            auto create_loader = checked(env, env->GetMethodID(loader_type, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V"));
            auto url = checked(env, env->FindClass("java/net/URL")); auto urls = checked(env, env->NewObjectArray(0, url, nullptr));
            auto loader = checked(env, env->NewObject(loader_type, create_loader, urls, parent));
            HMODULE owner = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&connect), &owner)) throw std::runtime_error("Client resource owner");
            auto resource = FindResourceW(owner, MAKEINTRESOURCEW(IDR_CLIENT_BRIDGE), RT_RCDATA);
            const auto size = resource ? SizeofResource(owner, resource) : 0;
            auto loaded = resource ? LoadResource(owner, resource) : nullptr; auto bytes = loaded ? LockResource(loaded) : nullptr;
            if (!bytes || !size || size > 0x7fffffffU) throw std::runtime_error("Client resource");
            auto type = checked(env, env->DefineClass("net/onestep/hybrid/ClientBridge", loader, static_cast<const jbyte*>(bytes), static_cast<jsize>(size)));
            auto create = checked(env, env->GetMethodID(type, "<init>", "(Ljava/lang/ClassLoader;I)V"));
            prepare_id = checked(env, env->GetMethodID(type, "prepare", "(I)V"));
            walk_id = checked(env, env->GetMethodID(type, "captureWalk", "()[Ljava/lang/Object;"));
            apply_walk_id = checked(env, env->GetMethodID(type, "applyWalk", "([Ljava/lang/Object;[Z)Z"));
            inputs_id = checked(env, env->GetMethodID(type, "walkInputs", "()[I"));
            release_walk_id = checked(env, env->GetMethodID(type, "releaseWalk", "(Z[I[Z)Z"));
            glow_id = checked(env, env->GetMethodID(type, "captureGlow", "()[Ljava/lang/Object;"));
            apply_glow_id = checked(env, env->GetMethodID(type, "applyGlow", "([Ljava/lang/Object;[Z)Z"));
            release_glow_id = checked(env, env->GetMethodID(type, "releaseGlow", "()Z"));
            stop_id = checked(env, env->GetMethodID(type, "stop", "()V"));
            set_inventory_busy_id = checked(env, env->GetMethodID(type, "setInventoryBusy", "(Z)V"));
            prepare_walk_action_id = checked(env, env->GetMethodID(type, "prepareWalkAction", "(II)Z"));
            auto object = checked(env, env->NewObject(type, create, parent, static_cast<jint>(mc::target.build_id)));
            bridge = checked(env, env->NewGlobalRef(object)); return true;
        } catch (...) {
            platform::jni_context::clear_exception(); failed = true;
            OutputDebugStringW(L"[OneStep] Client bridge setup unavailable; existing backends retained.\n"); return false;
        }
    }
    services::game::hybrid::result prepare(unsigned feature) {
        using services::game::hybrid::result;

        if constexpr (mc::target.build_id != 12111) return result::native_fallback;
        if (!permitted(feature) || !platform::jni_context::bind_current_thread() || suspended[feature]) return result::unavailable;
        if (rejected[feature] || !connect(jni::get_env())) return result::native_fallback;
        if (!prepared[feature]) {
            jni::get_env()->CallVoidMethod(bridge, prepare_id, static_cast<jint>(feature));
            if (platform::jni_context::clear_exception()) { rejected[feature] = true; return result::native_fallback; }
            prepared[feature] = true;
        }
        return result::ready;
    }
    void suspend(unsigned feature) {
        platform::jni_context::clear_exception(); suspended[feature] = true;
        OutputDebugStringW(L"[OneStep] Client capability suspended after runtime failure; cleanup only, no replay.\n");
    }
    bool result(unsigned feature, bool success) { if (!platform::jni_context::clear_exception()) return success; suspend(feature); return false; }
    bool physical(int kind, int code) {
        if (kind == 0) {
            if (code == 257) return services::input::physical_scan_down(0x1c);
            if (code == 335) return services::input::physical_scan_down(0x11c);
            auto vk = services::input::windows_key_for_glfw(code); return vk && services::input::physical_key_down(vk);
        }
        if (kind == 1) return code > 0 && code < 512 && services::input::physical_scan_down(static_cast<unsigned>(code));
        constexpr unsigned buttons[]{VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
        return kind == 2 && code >= 0 && code < 5 && services::input::physical_key_down(buttons[code]);
    }
    jobjectArray capture(jmethodID method, int size, int data_index, double* values, int length) {
        auto env = jni::get_env(); auto packet = static_cast<jobjectArray>(env->CallObjectMethod(bridge, method));
        if (env->ExceptionCheck()) throw std::runtime_error("Client capture");
        if (!packet) return nullptr;
        if (env->GetArrayLength(packet) != size || !env->IsSameObject(checked(env, env->GetObjectArrayElement(packet, 0)), bridge)) throw std::runtime_error("Client packet");
        auto raw = checked(env, env->GetObjectArrayElement(packet, data_index)); auto type = checked(env, env->FindClass("[D"));
        if (!env->IsInstanceOf(raw, type) || env->GetArrayLength(static_cast<jarray>(raw)) != length) throw std::runtime_error("Client values");
        env->GetDoubleArrayRegion(static_cast<jdoubleArray>(raw), 0, length, values);
        if (env->ExceptionCheck()) throw std::runtime_error("Client numeric read");
        for (int i = 0; i < length; ++i) if (!std::isfinite(values[i]) || std::floor(values[i]) != values[i]) throw std::runtime_error("Client number");
        if (values[0] != 1 || values[1] <= 0) throw std::runtime_error("Client schema");
        return packet;
    }
}

services::game::hybrid::result services::game::client_bridge::capture_walk(walk_snapshot& out)
{
    out = {}; auto backend = prepare(1); if (backend != hybrid::result::ready) return backend;
    try {
        std::array<double, 17> values{}; out.packet = capture(walk_id, 8, 7, values.data(), 17);
        if (!out.packet) return hybrid::result::unavailable;
        if (values[2] < 0 || values[2] > 1 || values[3] < 0 || values[3] > 3 || values[4] < 0 || values[4] > 1) throw std::runtime_error("Walk flags");
        out.alive = values[2] == 1; out.screen = static_cast<int>(values[3]); out.typing = values[4] == 1;
        for (unsigned i = 0; i < 6; ++i) {
            const double kind = values[5 + i * 2], code = values[6 + i * 2];
            if (kind < -1 || kind > 2 || code < (std::numeric_limits<int>::min)() || code > (std::numeric_limits<int>::max)()) throw std::runtime_error("Walk binding");
            out.descriptors[i * 2] = static_cast<int>(kind); out.descriptors[i * 2 + 1] = static_cast<int>(code);
            out.physical[i] = physical(static_cast<int>(kind), static_cast<int>(code));
        }
        return hybrid::result::ready;
    } catch (...) { suspend(1); out = {}; return hybrid::result::unavailable; }
}
bool services::game::client_bridge::apply_walk(const walk_snapshot& observed, const std::array<bool, 6>& desired)
{
    if (!bridge || !observed.packet || !permitted(1) || suspended[1]) return false;
    auto env = jni::get_env(); std::array<jboolean, 6> values{};
    for (unsigned i = 0; i < 6; ++i) values[i] = desired[i] && physical(observed.descriptors[i * 2], observed.descriptors[i * 2 + 1]);
    auto array = env->NewBooleanArray(6); if (!array || env->ExceptionCheck()) { suspend(1); return false; }
    env->SetBooleanArrayRegion(array, 0, 6, values.data());
    if (env->ExceptionCheck()) { suspend(1); return false; }
    return result(1, env->CallBooleanMethod(bridge, apply_walk_id, observed.packet, array) == JNI_TRUE);
}
bool services::game::client_bridge::prepare_walk_action(int kind, int code)
{
    if constexpr (mc::target.build_id != 12111) return true;
    if (!bridge || !prepared[1] || suspended[1]) return true;
    if (!permitted(1) || !platform::jni_context::bind_current_thread()) return false;
    jni::frame frame{64};auto env=jni::get_env();
    const bool ready=env->CallBooleanMethod(bridge,prepare_walk_action_id,static_cast<jint>(kind),static_cast<jint>(code))==JNI_TRUE;
    if (env->ExceptionCheck()) {
        std::fprintf(stderr,"[OneStep] Inventory Walk stopped before inventory action; Java synchronization failed.\n");
        env->ExceptionDescribe();
        platform::jni_context::clear_exception();suspend(1);release_walk(false);return false;
    }
    return ready;
}
bool services::game::client_bridge::release_walk(bool vanilla)
{
    if (!bridge) return true;
    if (!platform::jni_context::bind_current_thread()) return false;
    jni::frame frame{16}; auto env = jni::get_env();
    auto input = static_cast<jintArray>(env->CallObjectMethod(bridge, inputs_id));
    if (platform::jni_context::clear_exception()) return false;
    std::array<jint, 12> codes{}; codes.fill(-1);
    if (input) {
        if (env->GetArrayLength(input) != 12) return false;
        env->GetIntArrayRegion(input, 0, 12, codes.data());
        if (platform::jni_context::clear_exception()) return false;
    } else { input = env->NewIntArray(12); if (!input || env->ExceptionCheck()) { platform::jni_context::clear_exception(); return false; } env->SetIntArrayRegion(input, 0, 12, codes.data()); }
    if (platform::jni_context::clear_exception()) return false;
    std::array<jboolean, 6> held{};
    for (unsigned i = 0; i < 6; ++i) held[i] = physical(codes[i * 2], codes[i * 2 + 1]);
    auto states = env->NewBooleanArray(6);
    if (!states || env->ExceptionCheck()) { platform::jni_context::clear_exception(); return false; }
    env->SetBooleanArrayRegion(states, 0, 6, held.data());
    if (platform::jni_context::clear_exception()) return false;
    const bool released = env->CallBooleanMethod(bridge, release_walk_id, static_cast<jboolean>(vanilla), input, states) == JNI_TRUE;
    return !platform::jni_context::clear_exception() && released;
}
services::game::hybrid::result services::game::client_bridge::capture_glow(glow_snapshot& out)
{
    out = {}; auto backend = prepare(2); if (backend != hybrid::result::ready) return backend;
    try {
        std::array<double, 2> values{}; out.packet = capture(glow_id, 5, 4, values.data(), 2);
        if (!out.packet) return hybrid::result::unavailable;
        auto env = jni::get_env(); auto raw = checked(env, env->GetObjectArrayElement(out.packet, 3)); auto type = checked(env, env->FindClass("[Ljava/lang/Object;"));
        if (!env->IsInstanceOf(raw, type)) throw std::runtime_error("Glow candidates");
        out.players = static_cast<jobjectArray>(raw); out.count = env->GetArrayLength(out.players);
        if (out.count < 0 || out.count > 4096) throw std::runtime_error("Glow count");
        return hybrid::result::ready;
    } catch (...) { suspend(2); out = {}; return hybrid::result::unavailable; }
}
bool services::game::client_bridge::apply_glow(const glow_snapshot& observed, const std::vector<bool>& allowed)
{
    if (!bridge || !observed.packet || !permitted(2) || suspended[2] || allowed.size() != observed.count) return false;
    auto env = jni::get_env(); auto mask = env->NewBooleanArray(observed.count);
    if (!mask || env->ExceptionCheck()) { suspend(2); return false; }
    std::vector<jboolean> values(allowed.size()); for (unsigned i = 0; i < values.size(); ++i) values[i] = allowed[i];
    if (!values.empty()) env->SetBooleanArrayRegion(mask, 0, observed.count, values.data());
    if (env->ExceptionCheck()) { suspend(2); return false; }
    return result(2, env->CallBooleanMethod(bridge, apply_glow_id, observed.packet, mask) == JNI_TRUE);
}
bool services::game::client_bridge::release_glow()
{
    if (!bridge) return true;
    if (!platform::jni_context::bind_current_thread()) return false;
    const bool released = jni::get_env()->CallBooleanMethod(bridge, release_glow_id) == JNI_TRUE;
    return !platform::jni_context::clear_exception() && released;
}
void services::game::client_bridge::set_inventory_busy(bool busy)
{
    inventory_busy_state.store(busy, std::memory_order_release);
    if (!bridge || !set_inventory_busy_id || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{16};
    jni::get_env()->CallVoidMethod(bridge, set_inventory_busy_id, busy ? JNI_TRUE : JNI_FALSE);
    platform::jni_context::clear_exception();
}
bool services::game::client_bridge::is_inventory_busy()
{
    return inventory_busy_state.load(std::memory_order_acquire);
}
void services::game::client_bridge::shutdown()
{
    if (bridge && platform::jni_context::bind_current_thread()) {
        jni::get_env()->CallVoidMethod(bridge, stop_id); platform::jni_context::clear_exception(); jni::get_env()->DeleteGlobalRef(bridge);
    }
    bridge = nullptr; prepare_id = walk_id = apply_walk_id = inputs_id = release_walk_id = nullptr;
    glow_id = apply_glow_id = release_glow_id = stop_id = set_inventory_busy_id = prepare_walk_action_id = nullptr;
    inventory_busy_state.store(false);
    failed = false; prepared = {}; rejected = {}; suspended = {};
}
