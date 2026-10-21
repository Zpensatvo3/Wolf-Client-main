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
#include "../../modules/combat/trigger_engine.hpp"
#include "../../modules/combat/shield_breaker.hpp"
#include "../../modules/combat/mace_swap.hpp"
#include "../../friends/friends.hpp"
#include "anti_bot_service.hpp"
#include "attack_coordinator.hpp"
#include "client_context.hpp"

namespace
{
    jobject bridge = nullptr;
    jmethodID prepare_id = nullptr, edge_id = nullptr, support_id = nullptr, force_id = nullptr, input_id = nullptr, release_edge_id = nullptr;
    jmethodID pearl_id = nullptr, begin_id = nullptr, use_id = nullptr, release_pearl_id = nullptr, stop_id = nullptr;
    jmethodID crystal_tick_id = nullptr, crystal_cancel_id = nullptr, crystal_input_id = nullptr;
    jmethodID trigger_tick_id = nullptr, trigger_cancel_id = nullptr;
    std::array<bool, 5> prepared{}, rejected{}, suspended{};
    const std::atomic<std::uint64_t>* trigger_reset_source = nullptr;
    std::uint64_t trigger_expected_generation = 0;
    int trigger_helper = 0;
    bool failed = false;
    bool permitted() {
        const HWND window = gui::get_window();
        return auth::permitted(auth::scope::bridge) && window && GetForegroundWindow() == window &&
            !gui::draw && !gui::loader_visible && !gui::should_unload;
    }
    void finish_trigger_helper(jobject p, jobject w, bool consumed) {
        if (trigger_helper == 0) return;
        services::game::client_context context;
        services::game::capture_client_context(context, false, false);
        const bool same = p && w && context.player && context.world &&
            jni::get_env()->IsSameObject(p, context.player.object_instance) &&
            jni::get_env()->IsSameObject(w, context.world.object_instance);
        const maps::ClientPlayerEntity player = same ? context.player : maps::ClientPlayerEntity{};
        const maps::ClientPlayerInteractionManager manager = same ? context.interaction : maps::ClientPlayerInteractionManager{};
        if (trigger_helper == 2) modules::shield_breaker::finish_trigger_preparation(player, manager, consumed && same);
        if (trigger_helper == 3) modules::mace_swap::finish_trigger_preparation(player, manager, consumed && same);
        trigger_helper = 0;
    }
    jlong JNICALL trigger_service(JNIEnv* env, jobject, jint operation, jobject p, jobject w, jobject e, jint value, jlong epoch) {
        try {
            if (operation == 4 || operation == 5) {
                finish_trigger_helper(p, w, operation == 4);
                if (operation == 5) services::combat::release_trigger();
                return 1;
            }
            const bool valid = permitted() && trigger_reset_source &&
                trigger_reset_source->load(std::memory_order_acquire) == trigger_expected_generation;
            if (!valid) return operation == 8 || operation == 2 || operation == 3 || operation == 6 ? -1 : 0;
            switch (operation) {
            case 0: {
                // Shared OneStep filters remain the single source of truth.
                maps::Entity target(e, false);
                return target && !friends::is_friend(target) && !services::anti_bot::is_bot(target) &&
                    !platform::jni_context::clear_exception();
            }
            case 1: services::combat::claim_trigger(value, static_cast<std::uint64_t>(epoch)); return 1;
            case 2:
                trigger_helper = 2;
                if (modules::shield_breaker::prepare_for_trigger_attack()) return modules::shield_breaker::trigger_prepared_slot();
                return -1;
            case 3:
                trigger_helper = 3;
                if (modules::mace_swap::prepare_for_trigger_attack()) return modules::mace_swap::trigger_prepared_slot();
                return -1;
            case 6: {
                maps::ClientPlayerEntity player(p, false); maps::ClientWorld world(w, false);
                int slot = modules::shield_breaker::starting_slot_if_owned(player, world, value);
                return slot >= 0 ? slot : modules::mace_swap::starting_slot_if_owned(player, world, value);
            }
            case 7: return value == (trigger_helper == 2 ? modules::shield_breaker::trigger_prepared_slot() :
                trigger_helper == 3 ? modules::mace_swap::trigger_prepared_slot() : -1);
            case 8: return static_cast<std::uint64_t>(epoch) == trigger_expected_generation ?
                static_cast<jlong>(services::input::manual_attack_generation()) : -1;
            case 10: return -1; // Shield already handled by the selected axe.
            default: return 0;
            }
        } catch (...) {
            platform::jni_context::clear_exception();
            if (!env->ExceptionCheck()) {
                auto exception = env->FindClass("java/lang/IllegalStateException");
                if (exception) env->ThrowNew(exception, "Trigger shared service failed");
            }
            return -1;
        }
    }
    template<class T> T checked(JNIEnv* env, T value) {
        if (!value || env->ExceptionCheck()) throw std::runtime_error("World JNI binding");
        return value;
    }
    // Exact names for the selected binary, using the existing SDK profile.
    // Java-only thread/item/query methods and press count use official Mojang names on 26.1.x.
    jobjectArray crystal_names(JNIEnv* env)
    {
        struct binding { const char* source; const char* runtime; };
        static constexpr binding names[]{
            {"net/minecraft/class_312", ONESTEP_MC_NAME("net/minecraft/class_312")},
            {"field_1729", ONESTEP_MC_NAME("field_1729")},
            {"field_1791", ONESTEP_MC_NAME("field_1791")},
            {"method_1601", ONESTEP_MC_NAME("method_1601")},
#if ONESTEP_MINECRAFT_VERSION >= 12111
            {"net/minecraft/class_11910", ONESTEP_MC_NAME("net/minecraft/class_11910")},
#endif
#if ONESTEP_MINECRAFT_26_1
            {"field_1657", "ALL"},
            {"field_1653", "isDown"},
#else
            {"field_1657", "field_1657"},
            {"field_1653", "field_1653"},
#endif
            {"net/minecraft/class_1041", ONESTEP_MC_NAME("net/minecraft/class_1041")},
            {"method_22683", ONESTEP_MC_NAME("method_22683")},
            {"method_4490", ONESTEP_MC_NAME("method_4490")},
            {"net/minecraft/class_1309", ONESTEP_MC_NAME("net/minecraft/class_1309")},
            {"net/minecraft/class_1308", ONESTEP_MC_NAME("net/minecraft/class_1308")},
            {"net/minecraft/class_1429", ONESTEP_MC_NAME("net/minecraft/class_1429")},
            {"net/minecraft/class_1743", ONESTEP_MC_NAME("net/minecraft/class_1743")},
            {"net/minecraft/class_6880", ONESTEP_MC_NAME("net/minecraft/class_6880")},
            {"net/minecraft/class_1294", ONESTEP_MC_NAME("net/minecraft/class_1294")},
            {"field_6017", ONESTEP_MC_NAME("field_6017")},
            {"field_6273", ONESTEP_MC_NAME("field_6273")},
            {"field_6031", ONESTEP_MC_NAME("field_6031")},
            {"method_5767", ONESTEP_MC_NAME("method_5767")},
            {"method_7261", ONESTEP_MC_NAME("method_7261")},
            {"method_24828", ONESTEP_MC_NAME("method_24828")},
            {"method_5799", ONESTEP_MC_NAME("method_5799")},
            {"method_5681", ONESTEP_MC_NAME("method_5681")},
            {"method_6128", ONESTEP_MC_NAME("method_6128")},
            {"method_6101", ONESTEP_MC_NAME("method_6101")},
            {"method_5765", ONESTEP_MC_NAME("method_5765")},
            {"method_5624", ONESTEP_MC_NAME("method_5624")},
            {"method_6059", ONESTEP_MC_NAME("method_6059")},
            {"field_5919", ONESTEP_MC_NAME("field_5919")},
            {"method_6039", ONESTEP_MC_NAME("method_6039")},
            {"method_1560", ONESTEP_MC_NAME("method_1560")},
#if ONESTEP_MINECRAFT_VERSION >= 12111
            {"method_76763", ONESTEP_MC_NAME("method_76763")},
#else
            {"net/minecraft/class_757", ONESTEP_MC_NAME("net/minecraft/class_757")},
            {"field_1773", ONESTEP_MC_NAME("field_1773")},
            {"method_56153", ONESTEP_MC_NAME("method_56153")},
#endif
#if ONESTEP_MINECRAFT_VERSION < 12108
            {"net/minecraft/class_1829", ONESTEP_MC_NAME("net/minecraft/class_1829")},
#else
            {"net/minecraft/class_6880$class_6883", ONESTEP_MC_NAME("net/minecraft/class_6880$class_6883")},
            {"net/minecraft/class_6862", ONESTEP_MC_NAME("net/minecraft/class_6862")},
            {"net/minecraft/class_3489", ONESTEP_MC_NAME("net/minecraft/class_3489")},
            {"method_40131", ONESTEP_MC_NAME("method_40131")},
            {"method_40220", ONESTEP_MC_NAME("method_40220")},
            {"field_42611", ONESTEP_MC_NAME("field_42611")},
#endif

            {"net/minecraft/class_1268", ONESTEP_MC_NAME("net/minecraft/class_1268")},
            {"net/minecraft/class_1269", ONESTEP_MC_NAME("net/minecraft/class_1269")},
            {"net/minecraft/class_1297", ONESTEP_MC_NAME("net/minecraft/class_1297")},
            {"net/minecraft/class_1511", ONESTEP_MC_NAME("net/minecraft/class_1511")},
            {"net/minecraft/class_1657", ONESTEP_MC_NAME("net/minecraft/class_1657")},
            {"net/minecraft/class_1661", ONESTEP_MC_NAME("net/minecraft/class_1661")},
            {"net/minecraft/class_1747", ONESTEP_MC_NAME("net/minecraft/class_1747")},
            {"net/minecraft/class_1792", ONESTEP_MC_NAME("net/minecraft/class_1792")},
            {"net/minecraft/class_1799", ONESTEP_MC_NAME("net/minecraft/class_1799")},
            {"net/minecraft/class_1802", ONESTEP_MC_NAME("net/minecraft/class_1802")},
            {"net/minecraft/class_2246", ONESTEP_MC_NAME("net/minecraft/class_2246")},
            {"net/minecraft/class_2248", ONESTEP_MC_NAME("net/minecraft/class_2248")},
            {"net/minecraft/class_2338", ONESTEP_MC_NAME("net/minecraft/class_2338")},
            {"net/minecraft/class_2350", ONESTEP_MC_NAME("net/minecraft/class_2350")},
            {"net/minecraft/class_2371", ONESTEP_MC_NAME("net/minecraft/class_2371")},
            {"net/minecraft/class_238", ONESTEP_MC_NAME("net/minecraft/class_238")},
            {"net/minecraft/class_239", ONESTEP_MC_NAME("net/minecraft/class_239")},
            {"net/minecraft/class_239$class_240", ONESTEP_MC_NAME("net/minecraft/class_239$class_240")},
            {"net/minecraft/class_243", ONESTEP_MC_NAME("net/minecraft/class_243")},
            {"net/minecraft/class_2680", ONESTEP_MC_NAME("net/minecraft/class_2680")},
            {"net/minecraft/class_304", ONESTEP_MC_NAME("net/minecraft/class_304")},
            {"net/minecraft/class_310", ONESTEP_MC_NAME("net/minecraft/class_310")},
            {"net/minecraft/class_315", ONESTEP_MC_NAME("net/minecraft/class_315")},
            {"net/minecraft/class_3675$class_306", ONESTEP_MC_NAME("net/minecraft/class_3675$class_306")},
            {"net/minecraft/class_3675$class_307", ONESTEP_MC_NAME("net/minecraft/class_3675$class_307")},
            {"net/minecraft/class_3959", ONESTEP_MC_NAME("net/minecraft/class_3959")},
            {"net/minecraft/class_3959$class_242", ONESTEP_MC_NAME("net/minecraft/class_3959$class_242")},
            {"net/minecraft/class_3959$class_3960", ONESTEP_MC_NAME("net/minecraft/class_3959$class_3960")},
            {"net/minecraft/class_3965", ONESTEP_MC_NAME("net/minecraft/class_3965")},
            {"net/minecraft/class_3966", ONESTEP_MC_NAME("net/minecraft/class_3966")},
            {"net/minecraft/class_437", ONESTEP_MC_NAME("net/minecraft/class_437")},
            {"net/minecraft/class_636", ONESTEP_MC_NAME("net/minecraft/class_636")},
            {"net/minecraft/class_638", ONESTEP_MC_NAME("net/minecraft/class_638")},
            {"net/minecraft/class_746", ONESTEP_MC_NAME("net/minecraft/class_746")},
            {"field_10540", ONESTEP_MC_NAME("field_10540")},
            {"field_11036", ONESTEP_MC_NAME("field_11036")},
            {"field_1320", ONESTEP_MC_NAME("field_1320")},
            {"field_1321", ONESTEP_MC_NAME("field_1321")},
            {"field_1322", ONESTEP_MC_NAME("field_1322")},
            {"field_1323", ONESTEP_MC_NAME("field_1323")},
            {"field_1324", ONESTEP_MC_NAME("field_1324")},
            {"field_1325", ONESTEP_MC_NAME("field_1325")},
            {"field_1332", ONESTEP_MC_NAME("field_1332")},
            {"field_1348", ONESTEP_MC_NAME("field_1348")},
            {"field_1350", ONESTEP_MC_NAME("field_1350")},
            {"field_1351", ONESTEP_MC_NAME("field_1351")},
            {"field_1352", ONESTEP_MC_NAME("field_1352")},
            {"field_1655", ONESTEP_MC_NAME("field_1655")},
#if ONESTEP_MINECRAFT_26_1
            {"field_1661", "clickCount"},
#else
            {"field_1661", "field_1661"},
#endif
            {"field_1668", ONESTEP_MC_NAME("field_1668")},
            {"field_1671", ONESTEP_MC_NAME("field_1671")},
            {"field_1672", ONESTEP_MC_NAME("field_1672")},
            {"field_1687", ONESTEP_MC_NAME("field_1687")},
            {"field_1690", ONESTEP_MC_NAME("field_1690")},
            {"field_1700", ONESTEP_MC_NAME("field_1700")},
            {"field_1724", ONESTEP_MC_NAME("field_1724")},
            {"field_1755", ONESTEP_MC_NAME("field_1755")},
            {"field_17558", ONESTEP_MC_NAME("field_17558")},
            {"field_17592", ONESTEP_MC_NAME("field_17592")},
            {"field_1761", ONESTEP_MC_NAME("field_1761")},
            {"field_1765", ONESTEP_MC_NAME("field_1765")},
            {"field_1904", ONESTEP_MC_NAME("field_1904")},
            {"field_22467", ONESTEP_MC_NAME("field_22467")},
            {"field_5808", ONESTEP_MC_NAME("field_5808")},
            {"field_7545", ONESTEP_MC_NAME("field_7545")},
            {"field_8301", ONESTEP_MC_NAME("field_8301")},
            {"field_9987", ONESTEP_MC_NAME("field_9987")},
            {"method_10260", ONESTEP_MC_NAME("method_10260")},
            {"method_10263", ONESTEP_MC_NAME("method_10263")},
            {"method_10264", ONESTEP_MC_NAME("method_10264")},
            {"method_1434", ONESTEP_MC_NAME("method_1434")},
            {"method_1442", ONESTEP_MC_NAME("method_1442")},
            {"method_1444", ONESTEP_MC_NAME("method_1444")},
            {"method_17742", ONESTEP_MC_NAME("method_17742")},
            {"method_17777", ONESTEP_MC_NAME("method_17777")},
            {"method_17783", ONESTEP_MC_NAME("method_17783")},
#if ONESTEP_MINECRAFT_26_1
            {"method_18854", "isSameThread"},
#else
            {"method_18854", "method_18854"},
#endif
            {"method_23320", ONESTEP_MC_NAME("method_23320")},
            {"method_23481", ONESTEP_MC_NAME("method_23481")},
            {"method_23665", ONESTEP_MC_NAME("method_23665")},
            {"method_26215", ONESTEP_MC_NAME("method_26215")},
            {"method_27852", ONESTEP_MC_NAME("method_27852")},
            {"method_2896", ONESTEP_MC_NAME("method_2896")},
            {"method_2911", ONESTEP_MC_NAME("method_2911")},
            {"method_2918", ONESTEP_MC_NAME("method_2918")},
            {"method_31548", ONESTEP_MC_NAME("method_31548")},
            {"method_5438", ONESTEP_MC_NAME("method_5438")},
            {"method_55754", ONESTEP_MC_NAME("method_55754")},
            {"method_55755", ONESTEP_MC_NAME("method_55755")},
            {"method_5628", ONESTEP_MC_NAME("method_5628")},
            {"method_5805", ONESTEP_MC_NAME("method_5805")},
            {"method_5829", ONESTEP_MC_NAME("method_5829")},
            {"method_6104", ONESTEP_MC_NAME("method_6104")},
            {"method_6115", ONESTEP_MC_NAME("method_6115")},
#if ONESTEP_MINECRAFT_26_1
            {"method_7909", "getItem"},
#else
            {"method_7909", "method_7909"},
#endif
            {"method_7960", ONESTEP_MC_NAME("method_7960")},
            {"method_8320", ONESTEP_MC_NAME("method_8320")},
#if ONESTEP_MINECRAFT_26_1
            {"method_8335", "getEntities"},
#else
            {"method_8335", "method_8335"},
#endif
            {"method_8469", ONESTEP_MC_NAME("method_8469")},
        };
        auto string_type = checked(env, env->FindClass("java/lang/String"));
        auto values = checked(env, env->NewObjectArray(static_cast<jsize>(std::size(names)*2), string_type, nullptr));
        jsize index = 0;
        for (const auto& name : names)
        {
            for (const auto text : {name.source, name.runtime})
            {
                auto value = checked(env, env->NewStringUTF(text));
                env->SetObjectArrayElement(values, index++, value);
                env->DeleteLocalRef(value);
                if (env->ExceptionCheck()) throw std::runtime_error("World mapping array");
            }
        }
        return values;
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
            JNINativeMethod callbacks[]{
                {const_cast<char*>("triggerService"), const_cast<char*>("(ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;IJ)J"), reinterpret_cast<void*>(&trigger_service)}
            };
            if (env->RegisterNatives(type, callbacks, 1) != JNI_OK || env->ExceptionCheck()) throw std::runtime_error("Trigger native service binding");
            trigger_tick_id = checked(env, env->GetMethodID(type, "tickTrigger", "(ZJJFFIIIIIZZZ)V"));
            trigger_cancel_id = checked(env, env->GetMethodID(type, "cancelTrigger", "()V"));
            auto create = checked(env, env->GetMethodID(type, "<init>", "(Ljava/lang/ClassLoader;I[Ljava/lang/String;)V"));
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
            crystal_tick_id = checked(env, env->GetMethodID(type, "tickCrystal", "(ZZIIZZZII)V"));
            crystal_cancel_id = checked(env, env->GetMethodID(type, "cancelCrystal", "(IIZ)Z"));
            crystal_input_id = checked(env, env->GetMethodID(type, "crystalInput", "()[I"));
            auto object = checked(env, env->NewObject(type, create, parent, static_cast<jint>(mc::target.build_id), crystal_names(env)));
            bridge = checked(env, env->NewGlobalRef(object)); return true;
        } catch (...) {
            platform::jni_context::clear_exception(); failed = true;
            OutputDebugStringW(L"[OneStep] World bridge setup unavailable; Java-only combat controllers inactive.\n"); return false;
        }
    }
    services::game::hybrid::result prepare(unsigned feature) {
        using services::game::hybrid::result;
        if (!auth::permitted(auth::scope::bridge)) return result::unavailable;
        if (feature < 1 || feature > 4) return result::unavailable;
        if constexpr (mc::target.build_id != 12111) if (feature < 3) return result::native_fallback;
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
void services::game::world_bridge::tick_crystal(bool active, bool auto_switch, bool restore_slot, int place_delay, int break_delay)
{
    if (!active || !permitted() || !auth::permitted(auth::scope::input)) { cancel_crystal(); return; }
    if (!platform::jni_context::bind_current_thread()) return;
    jni::frame frame{48};
    const bool held = services::input::physical_button_down(services::input::mouse_button::right);
    if (!bridge && !held) return;
    if (prepare(3) != hybrid::result::ready) { cancel_crystal(); return; }
    auto env = jni::get_env();
    auto descriptor = static_cast<jintArray>(env->CallObjectMethod(bridge, crystal_input_id));
    if (platform::jni_context::clear_exception()) { suspended[3] = true; cancel_crystal(); return; }
    if (!descriptor) return; // Not the client thread or no options; never enqueue work.
    if (env->GetArrayLength(descriptor) != 2) { suspended[3] = true; cancel_crystal(); return; }
    std::array<jint, 2> input{-1, -1}; env->GetIntArrayRegion(descriptor, 0, 2, input.data());
    if (platform::jni_context::clear_exception()) { suspended[3] = true; cancel_crystal(); return; }
    env->CallVoidMethod(bridge, crystal_tick_id, JNI_TRUE, static_cast<jboolean>(held), input[0], input[1],
        static_cast<jboolean>(physical(input[0], input[1])),
        static_cast<jboolean>(auto_switch), static_cast<jboolean>(restore_slot), static_cast<jint>(place_delay), static_cast<jint>(break_delay));
    if (platform::jni_context::clear_exception()) {
        suspended[3] = true;
        cancel_crystal();
        OutputDebugStringW(L"[OneStep] Hit Crystal Java controller suspended; no native replay.\n");
    }
}
void services::game::world_bridge::cancel_crystal()
{
    if (!bridge || !prepared[3] || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{16};
    auto env = jni::get_env();
    auto descriptor = static_cast<jintArray>(env->CallObjectMethod(bridge, crystal_input_id));
    if (platform::jni_context::clear_exception()) return;
    std::array<jint, 2> input{-1, -1};
    if (descriptor) {
        if (env->GetArrayLength(descriptor) != 2) return;
        env->GetIntArrayRegion(descriptor, 0, 2, input.data());
        if (platform::jni_context::clear_exception()) return;
    }
    const HWND window = gui::get_window();
    const bool held = window && GetForegroundWindow() == window && physical(input[0], input[1]);
    env->CallBooleanMethod(bridge, crystal_cancel_id, input[0], input[1], static_cast<jboolean>(held));
    platform::jni_context::clear_exception();
}
void services::game::world_bridge::tick_trigger(const trigger_engine::config& settings, bool active,
    const std::atomic<std::uint64_t>& generation)
{
    if (!active || !permitted()) { cancel_trigger(); return; }
    if (!platform::jni_context::bind_current_thread()) { services::combat::release_trigger(); return; }
    jni::frame frame{192};
    if (prepare(4) != hybrid::result::ready) { cancel_trigger(); return; }
    trigger_expected_generation = generation.load(std::memory_order_acquire);
    trigger_reset_source = &generation;
    auto env = jni::get_env();
    env->CallVoidMethod(bridge, trigger_tick_id, JNI_TRUE,
        static_cast<jlong>(trigger_expected_generation), static_cast<jlong>(services::input::manual_attack_generation()),
        static_cast<jfloat>(settings.cooldown[0]), static_cast<jfloat>(settings.cooldown[1]),
        static_cast<jint>(settings.reaction[0]), static_cast<jint>(settings.reaction[1]),
        static_cast<jint>(settings.targets), static_cast<jint>(settings.weapon), static_cast<jint>(settings.criticals),
        static_cast<jboolean>(settings.on_click), static_cast<jboolean>(settings.swing_hand),
        static_cast<jboolean>(settings.click_simulation));
    if (!checked_result(4, true)) cancel_trigger();
}
void services::game::world_bridge::cancel_trigger()
{
    trigger_reset_source = nullptr;
    trigger_expected_generation = 0;
    services::combat::release_trigger();
    if (!bridge || !trigger_cancel_id || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{96};
    jni::get_env()->CallVoidMethod(bridge, trigger_cancel_id);
    if (platform::jni_context::clear_exception()) {
        finish_trigger_helper(nullptr, nullptr, false);
        suspended[4] = true;
    }
}
void services::game::world_bridge::shutdown()
{
    if (bridge && platform::jni_context::bind_current_thread()) {
        jni::get_env()->CallVoidMethod(bridge, stop_id); platform::jni_context::clear_exception();
        auto type = jni::get_env()->GetObjectClass(bridge);
        if (type) { jni::get_env()->UnregisterNatives(type); jni::get_env()->DeleteLocalRef(type); }
        platform::jni_context::clear_exception(); jni::get_env()->DeleteGlobalRef(bridge);
    }
    bridge = nullptr; prepare_id = edge_id = support_id = force_id = input_id = release_edge_id = nullptr;
    pearl_id = begin_id = use_id = release_pearl_id = stop_id = nullptr;
    crystal_tick_id = crystal_cancel_id = crystal_input_id = nullptr;
    trigger_tick_id = trigger_cancel_id = nullptr; trigger_reset_source = nullptr; trigger_helper = 0;
    services::combat::release_trigger();
    prepared = {}; rejected = {}; suspended = {}; failed = false;
}
