#include "world_bridge.hpp"
#include "../../auth/client.hpp"
#include "../../auth/auth.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../input/input_service.hpp"
#include "../../sdk/minecraft/common/client.hpp"
#include "../../../resource.h"
#include <Windows.h>
#include <cmath>
#include <stdexcept>

namespace
{
    jobject bridge = nullptr;
    jmethodID prepare_id = nullptr, edge_id = nullptr, support_id = nullptr, force_id = nullptr, input_id = nullptr, release_edge_id = nullptr;
    jmethodID pearl_id = nullptr, begin_id = nullptr, use_id = nullptr, release_pearl_id = nullptr, stop_id = nullptr;
    std::array<bool, 3> prepared{}, rejected{}, suspended{};
    bool failed = false;
    bool permitted() {
        const HWND window = gui::get_window();
        return auth::permitted(auth::scope::bridge) && window && GetForegroundWindow() == window &&
            !gui::draw && !gui::loader_visible && !gui::should_unload;
    }
    template<class T> T checked(JNIEnv* env, T value) {
        if (!value || env->ExceptionCheck()) throw std::runtime_error("World JNI binding");
        return value;
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
            auto url = checked(env, env->FindClass("java/net/URL"));
            auto urls = checked(env, env->NewObjectArray(0, url, nullptr));
            auto loader = checked(env, env->NewObject(loader_type, create_loader, urls, parent));
            HMODULE owner = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&connect), &owner)) throw std::runtime_error("World resource owner");
            auto resource = FindResourceW(owner, MAKEINTRESOURCEW(IDR_WORLD_BRIDGE), RT_RCDATA);
            const auto size = resource ? SizeofResource(owner, resource) : 0;
            auto loaded = resource ? LoadResource(owner, resource) : nullptr;
            auto bytes = loaded ? LockResource(loaded) : nullptr;
            if (!bytes || !size || size > 0x7fffffffU) throw std::runtime_error("World resource");
            auto type = checked(env, env->DefineClass("net/onestep/hybrid/WorldBridge", loader, static_cast<const jbyte*>(bytes), static_cast<jsize>(size)));
            auto create = checked(env, env->GetMethodID(type, "<init>", "(Ljava/lang/ClassLoader;I)V"));
            prepare_id = checked(env, env->GetMethodID(type, "prepare", "(I)V"));
            edge_id = checked(env, env->GetMethodID(type, "captureBridge", "()[Ljava/lang/Object;"));
            support_id = checked(env, env->GetMethodID(type, "support", "([Ljava/lang/Object;[I)[I"));
            force_id = checked(env, env->GetMethodID(type, "forceSneak", "([Ljava/lang/Object;)Z"));
            input_id = checked(env, env->GetMethodID(type, "sneakInput", "()[I"));
            release_edge_id = checked(env, env->GetMethodID(type, "releaseSneak", "(IIZ)Z"));
            pearl_id = checked(env, env->GetMethodID(type, "capturePearl", "()[Ljava/lang/Object;"));
            begin_id = checked(env, env->GetMethodID(type, "beginPearl", "([Ljava/lang/Object;I)Z"));
            use_id = checked(env, env->GetMethodID(type, "usePearl", "()Z"));
            release_pearl_id = checked(env, env->GetMethodID(type, "releasePearl", "()Z"));
            stop_id = checked(env, env->GetMethodID(type, "stop", "()V"));
            auto object = checked(env, env->NewObject(type, create, parent, static_cast<jint>(mc::target.build_id)));
            bridge = checked(env, env->NewGlobalRef(object)); return true;
        } catch (...) {
            platform::jni_context::clear_exception(); failed = true;
            OutputDebugStringW(L"[OneStep] World bridge setup unavailable; existing backends retained.\n"); return false;
        }
    }
    services::game::hybrid::result prepare(unsigned feature) {
        using services::game::hybrid::result;
        if (!auth::permitted(auth::scope::bridge)) return result::unavailable;
        if constexpr (mc::target.build_id != 12111) return result::native_fallback;
        if (!permitted() || !platform::jni_context::bind_current_thread() || suspended[feature]) return result::unavailable;
        if (rejected[feature] || !connect(jni::get_env())) return result::native_fallback;
        if (!prepared[feature]) {
            jni::get_env()->CallVoidMethod(bridge, prepare_id, static_cast<jint>(feature));
            if (platform::jni_context::clear_exception()) { rejected[feature] = true; return result::native_fallback; }
            prepared[feature] = true;
        }
        return result::ready;
    }
    bool checked_result(unsigned feature, bool value) {
        if (!platform::jni_context::clear_exception()) return value;
        suspended[feature] = true;
        OutputDebugStringW(L"[OneStep] World capability suspended after runtime failure; no action replay.\n");
        return false;
    }
    jobjectArray packet(unsigned feature, jmethodID id, int size, int data_index, std::array<double, 13>& values) {
        auto env = jni::get_env();
        auto p = static_cast<jobjectArray>(env->CallObjectMethod(bridge, id));
        if (env->ExceptionCheck()) throw std::runtime_error("World capture");
        if (!p) return nullptr;
        if (env->GetArrayLength(p) != size || !env->IsSameObject(checked(env, env->GetObjectArrayElement(p, 0)), bridge)) throw std::runtime_error("World packet");
        auto raw = checked(env, env->GetObjectArrayElement(p, data_index));
        auto doubles = checked(env, env->FindClass("[D"));
        if (!env->IsInstanceOf(raw, doubles) || env->GetArrayLength(static_cast<jarray>(raw)) != 13) throw std::runtime_error("World values");
        env->GetDoubleArrayRegion(static_cast<jdoubleArray>(raw), 0, 13, values.data());
        for (double v : values) if (!std::isfinite(v)) throw std::runtime_error("World number");
        if (env->ExceptionCheck() || values[0] != 1 || values[1] <= 0 || std::floor(values[1]) != values[1]) throw std::runtime_error("World schema");
        const unsigned start = feature == 1 ? 9 : 3;
        for (unsigned i = start; i < 13; ++i) if (values[i] != 0 && values[i] != 1) throw std::runtime_error("World flag");
        return p;
    }
    bool physical(int kind, int code) {
        if (kind == 0) {
            if (code == 257) return services::input::physical_scan_down(0x1c);
            if (code == 335) return services::input::physical_scan_down(0x11c);
            auto vk = services::input::windows_key_for_glfw(code);
            return vk && services::input::physical_key_down(vk);
        }
        if (kind == 1) return code > 0 && code < 512 && services::input::physical_scan_down(static_cast<unsigned>(code));
        constexpr unsigned buttons[]{VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
        return kind == 2 && code >= 0 && code < 5 && services::input::physical_key_down(buttons[code]);
    }
}

services::game::hybrid::result services::game::world_bridge::capture_edge(edge_snapshot& out)
{
    out = {}; const auto backend = prepare(1); if (backend != hybrid::result::ready) return backend;
    try {
        out.packet = packet(1, edge_id, 8, 7, out.data);
        if (!out.packet) return hybrid::result::unavailable;
        auto env = jni::get_env();
        out.player = checked(env, env->GetObjectArrayElement(out.packet, 1)); out.world = checked(env, env->GetObjectArrayElement(out.packet, 2));
        out.key = checked(env, env->GetObjectArrayElement(out.packet, 4)); out.binding = checked(env, env->GetObjectArrayElement(out.packet, 5));
        return hybrid::result::ready;
    } catch (...) { checked_result(1, false); suspended[1] = true; out = {}; return hybrid::result::unavailable; }
}
bool services::game::world_bridge::support(const edge_snapshot& observed, const std::array<int, 24>& positions, std::array<int, 8>& air)
{
    if (!bridge || !observed.packet || !permitted() || suspended[1]) return false;
    auto env = jni::get_env();
    auto points = env->NewIntArray(24);
    if (!points || env->ExceptionCheck()) return checked_result(1, false);
    std::array<jint, 24> native_points{};
    for (unsigned i = 0; i < positions.size(); ++i) native_points[i] = static_cast<jint>(positions[i]);
    env->SetIntArrayRegion(points, 0, 24, native_points.data());
    if (env->ExceptionCheck()) return checked_result(1, false);
    auto values = static_cast<jintArray>(env->CallObjectMethod(bridge, support_id, observed.packet, points));
    if (!checked_result(1, values != nullptr)) return false;
    if (env->GetArrayLength(values) != 8) { suspended[1] = true; return false; }
    std::array<jint, 8> raw{}; env->GetIntArrayRegion(values, 0, 8, raw.data());
    if (!checked_result(1, true)) return false;
    for (unsigned i = 0; i < 8; ++i) { if (raw[i] < 0 || raw[i] > 1) { suspended[1] = true; return false; } air[i] = raw[i]; }
    return true;
}
bool services::game::world_bridge::force_sneak(const edge_snapshot& observed)
{
    if (!bridge || !observed.packet || !permitted() || suspended[1]) return false;
    return checked_result(1, jni::get_env()->CallBooleanMethod(bridge, force_id, observed.packet) == JNI_TRUE);
}
bool services::game::world_bridge::release_sneak()
{
    if (!bridge) return true;
    if (!platform::jni_context::bind_current_thread()) return false;
    jni::frame frame{12}; auto env = jni::get_env();
    auto descriptor = static_cast<jintArray>(env->CallObjectMethod(bridge, input_id));
    if (platform::jni_context::clear_exception()) return false;
    std::array<jint, 2> input{-1, -1};
    if (descriptor) {
        if (env->GetArrayLength(descriptor) != 2) return false;
        env->GetIntArrayRegion(descriptor, 0, 2, input.data());
        if (platform::jni_context::clear_exception()) return false;
    }
    const bool result = env->CallBooleanMethod(bridge, release_edge_id, input[0], input[1], static_cast<jboolean>(physical(input[0], input[1]))) == JNI_TRUE;
    return !platform::jni_context::clear_exception() && result;
}
services::game::hybrid::result services::game::world_bridge::capture_pearl(pearl_snapshot& out)
{
    out = {}; const auto backend = prepare(2); if (backend != hybrid::result::ready) return backend;
    try {
        std::array<double, 13> values{}; out.packet = packet(2, pearl_id, 7, 6, values);
        if (!out.packet) return hybrid::result::unavailable;
        if (values[2] < 0 || values[2] > 8 || std::floor(values[2]) != values[2]) throw std::runtime_error("Pearl slot");
        out.selected = static_cast<int>(values[2]); out.owned = values[3] == 1;
        for (unsigned i = 0; i < 9; ++i) out.pearls[i] = values[4 + i] == 1;
        return hybrid::result::ready;
    } catch (...) { checked_result(2, false); suspended[2] = true; out = {}; return hybrid::result::unavailable; }
}
bool services::game::world_bridge::begin_pearl(const pearl_snapshot& observed, int slot)
{
    if (!bridge || !observed.packet || !permitted() || suspended[2]) return false;
    return checked_result(2, jni::get_env()->CallBooleanMethod(bridge, begin_id, observed.packet, static_cast<jint>(slot)) == JNI_TRUE);
}
bool services::game::world_bridge::use_pearl()
{
    return bridge && permitted() && !suspended[2] && checked_result(2, jni::get_env()->CallBooleanMethod(bridge, use_id) == JNI_TRUE);
}
bool services::game::world_bridge::release_pearl()
{
    if (!bridge) return true;
    if (!platform::jni_context::bind_current_thread()) return false;
    const bool released = jni::get_env()->CallBooleanMethod(bridge, release_pearl_id) == JNI_TRUE;
    return !platform::jni_context::clear_exception() && released;
}
void services::game::world_bridge::shutdown()
{
    if (bridge && platform::jni_context::bind_current_thread()) {
        jni::get_env()->CallVoidMethod(bridge, stop_id); platform::jni_context::clear_exception(); jni::get_env()->DeleteGlobalRef(bridge);
    }
    bridge = nullptr; prepare_id = edge_id = support_id = force_id = input_id = release_edge_id = nullptr;
    pearl_id = begin_id = use_id = release_pearl_id = stop_id = nullptr;
    prepared = {}; rejected = {}; suspended = {}; failed = false;
}
