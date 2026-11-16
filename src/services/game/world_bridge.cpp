#include "world_bridge.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../input/input_service.hpp"
#include "../../sdk/minecraft/common/client.hpp"
#include "../../../resource.h"
#include <Windows.h>
#include <cmath>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <jvmti.h>

extern JavaVM* jvm;
#include "../../modules/combat/safe_anchor.hpp"
#include "../../modules/combat/trigger_engine.hpp"
#include "../../modules/combat/jump_reset_config.hpp"
#include "../../modules/combat/settings/anchor_macro_settings.hpp"
#include "../../modules/combat/settings/auto_totem_settings.hpp"
#include "../../modules/combat/settings/crystal_aura_settings.hpp"
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
    jmethodID jump_tick_id = nullptr, jump_cancel_id = nullptr;
    const std::atomic<std::uint64_t>* jump_reset_source = nullptr;
    std::uint64_t jump_expected_generation = 0;
    jmethodID anchor_tick_id = nullptr, anchor_cancel_id = nullptr;
    jmethodID set_safe_anchor_active_id = nullptr, is_safe_anchor_active_id = nullptr;
    std::atomic_bool safe_anchor_active_state{false};
    void JNICALL safe_anchor_native_tick(JNIEnv* env, jobject self);
    const std::atomic<std::uint64_t>* anchor_reset_source = nullptr;
    std::uint64_t anchor_expected_generation = 0;
    jmethodID totem_tick_id = nullptr, totem_cancel_id = nullptr;
    const std::atomic<std::uint64_t>* totem_reset_source = nullptr;
    std::uint64_t totem_expected_generation = 0;
    std::array<bool, 9> prepared{}, rejected{}, suspended{};
    jmethodID aura_tick_id = nullptr, aura_cancel_id = nullptr, aura_action_id = nullptr;
    const std::atomic<std::uint64_t>* aura_reset_source = nullptr;
    std::uint64_t aura_expected_generation = 0;
    const std::atomic<std::uint64_t>* trigger_reset_source = nullptr;
    std::uint64_t trigger_expected_generation = 0;
    int trigger_helper = 0;
    bool failed = false;
    void report_java_failure(JNIEnv* env, const char* operation)
    {
        std::fprintf(stderr,"[OneStep] WorldBridge %s failed; Java combat controllers paused.\n",operation);
        if (env && env->ExceptionCheck()) env->ExceptionDescribe();
    }
    jvmtiEnv* anchor_tooling = nullptr;
    jclass anchor_client_class = nullptr, anchor_transformer_class = nullptr;
    jmethodID anchor_transform_id = nullptr, anchor_install_events_id = nullptr;
    std::atomic_bool anchor_transforming{false};
    bool anchor_hooks_installed = false, anchor_hooks_failed = false, anchor_hook_changed = false;
    void JNICALL anchor_class_bytes(jvmtiEnv* tooling, JNIEnv* env, jclass redefining, jobject,
        const char* name, jobject, jint size, const unsigned char* bytes, jint* new_size, unsigned char** new_bytes)
    {
        if (!anchor_transforming.load(std::memory_order_acquire) || !redefining || !name ||
            std::strcmp(name,ONESTEP_MC_NAME("net/minecraft/class_310")) != 0 || !env->IsSameObject(redefining,anchor_client_class)) return;
        if (env->PushLocalFrame(8) != JNI_OK) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
        auto original = env->NewByteArray(size);
        if (original) env->SetByteArrayRegion(original,0,size,reinterpret_cast<const jbyte*>(bytes));
        auto result = original && !env->ExceptionCheck() ? static_cast<jbyteArray>(env->CallStaticObjectMethod(
            anchor_transformer_class,anchor_transform_id,original,static_cast<jint>(mc::target.build_id))) : nullptr;
        if (result && !env->ExceptionCheck()) {
            const auto count = env->GetArrayLength(result); unsigned char* copy = nullptr;
            if (count > 0 && count < size*4+65536 && tooling->Allocate(count,&copy) == JVMTI_ERROR_NONE) {
                env->GetByteArrayRegion(result,0,count,reinterpret_cast<jbyte*>(copy));
                if (!env->ExceptionCheck()) { *new_size=count; *new_bytes=copy; anchor_hook_changed=true; }
                else tooling->Deallocate(copy);
            }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->PopLocalFrame(nullptr);
    }
    void shutdown_anchor_hooks(JNIEnv* env)
    {
        anchor_transforming.store(false,std::memory_order_release);
        if (anchor_tooling) {
            anchor_tooling->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            if (anchor_hook_changed && anchor_client_class) anchor_tooling->RetransformClasses(1,&anchor_client_class);
            anchor_tooling->DisposeEnvironment(); anchor_tooling=nullptr;
        }
        platform::jni_context::clear_exception();
        for (auto* ref : {&anchor_client_class,&anchor_transformer_class}) if (*ref) { env->DeleteGlobalRef(*ref); *ref=nullptr; }
        anchor_transform_id=nullptr; anchor_hooks_installed=anchor_hook_changed=false;
    }
    template<class T> T anchor_checked(JNIEnv* env,T value) {
        if (!value || env->ExceptionCheck()) throw std::runtime_error("Anchor event binding");
        return value;
    }
    jclass define_anchor_class(JNIEnv* env,jobject loader,const char* binary,int resource_id)
    {
        HMODULE owner=nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&define_anchor_class),&owner)) throw std::runtime_error("Anchor resource owner");
        auto resource=FindResourceW(owner,MAKEINTRESOURCEW(resource_id),RT_RCDATA);
        auto size=resource ? SizeofResource(owner,resource) : 0;
        auto loaded=resource ? LoadResource(owner,resource) : nullptr;
        auto bytes=loaded ? LockResource(loaded) : nullptr;
        if (!bytes || !size || size > 0x7fffffffU) throw std::runtime_error("Anchor event resource");
        return anchor_checked(env,env->DefineClass(binary,loader,static_cast<const jbyte*>(bytes),static_cast<jsize>(size)));
    }
    bool install_anchor_hooks(JNIEnv* env)
    {
        if (anchor_hooks_installed) return true;
        if (anchor_hooks_failed || !jvm) return false;
        try {
            auto client=anchor_checked(env,jni::get_cached_jclass<maps::MinecraftClient>());
            auto klass=anchor_checked(env,env->FindClass("java/lang/Class"));
            auto get_loader=anchor_checked(env,env->GetMethodID(klass,"getClassLoader","()Ljava/lang/ClassLoader;"));
            auto parent=anchor_checked(env,env->CallObjectMethod(client,get_loader));
            auto for_name=anchor_checked(env,env->GetStaticMethodID(klass,"forName","(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;"));
            auto events_name=anchor_checked(env,env->NewStringUTF("net.onestep.hybrid.AnchorEvents"));
            auto events=static_cast<jclass>(env->CallStaticObjectMethod(klass,for_name,events_name,JNI_TRUE,parent));
            if (platform::jni_context::clear_exception()) events=nullptr;
            if (!events) define_anchor_class(env,parent,"net/onestep/hybrid/AnchorEvents",IDR_ANCHOR_EVENTS);
            auto bridge_type=anchor_checked(env,env->GetObjectClass(bridge));
            auto loader=anchor_checked(env,env->CallObjectMethod(bridge_type,get_loader));
            auto transformer=define_anchor_class(env,loader,"net/onestep/hybrid/AnchorTransformer",IDR_ANCHOR_TRANSFORMER);
            anchor_transformer_class=static_cast<jclass>(anchor_checked(env,env->NewGlobalRef(transformer)));
            anchor_client_class=static_cast<jclass>(anchor_checked(env,env->NewGlobalRef(client)));
            anchor_transform_id=anchor_checked(env,env->GetStaticMethodID(transformer,"transform","([BI)[B"));
            if (jvm->GetEnv(reinterpret_cast<void**>(&anchor_tooling),JVMTI_VERSION_1_2) != JNI_OK || !anchor_tooling)
                throw std::runtime_error("Anchor JVMTI environment");
            jvmtiCapabilities caps{}; caps.can_retransform_classes=1;
            if (anchor_tooling->AddCapabilities(&caps) != JVMTI_ERROR_NONE) throw std::runtime_error("Anchor retransform capability");
            jboolean modifiable=JNI_FALSE;
            if (anchor_tooling->IsModifiableClass(client,&modifiable) != JVMTI_ERROR_NONE || !modifiable) throw std::runtime_error("Anchor client modifiability");
            jvmtiEventCallbacks callbacks{}; callbacks.ClassFileLoadHook=&anchor_class_bytes;
            if (anchor_tooling->SetEventCallbacks(&callbacks,sizeof(callbacks)) != JVMTI_ERROR_NONE) throw std::runtime_error("Anchor callback");
            anchor_transforming.store(true,std::memory_order_release);
            auto result=anchor_tooling->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            if (result == JVMTI_ERROR_NONE) result=anchor_tooling->RetransformClasses(1,&anchor_client_class);
            anchor_tooling->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            anchor_transforming.store(false,std::memory_order_release);
            if (result != JVMTI_ERROR_NONE || !anchor_hook_changed) throw std::runtime_error("Anchor client hooks");
            env->CallVoidMethod(bridge,anchor_install_events_id);
            if (env->ExceptionCheck()) throw std::runtime_error("Anchor Java event handlers");
            anchor_hooks_installed=true; return true;
        } catch (...) {
            platform::jni_context::clear_exception(); shutdown_anchor_hooks(env); anchor_hooks_failed=true;
            OutputDebugStringW(L"[OneStep] Combat event hooks unavailable; Auto Totem and Legit Anchor inactive. Normal Anchor remains available.\n");
            return false;
        }
    }
    bool permitted() {
        const HWND window = gui::get_window();
        return window && GetForegroundWindow() == window &&
            !gui::draw && !gui::loader_visible && !gui::should_unload;
    }
    jboolean JNICALL jump_permitted(JNIEnv*, jobject, jlong generation) {
        return permitted() && jump_reset_source && static_cast<std::uint64_t>(generation) == jump_expected_generation &&
            jump_reset_source->load(std::memory_order_acquire) == jump_expected_generation ? JNI_TRUE : JNI_FALSE;
    }
    jboolean JNICALL anchor_permitted(JNIEnv*, jobject, jlong generation) {
        return permitted() && anchor_reset_source && static_cast<std::uint64_t>(generation) == anchor_expected_generation &&
            anchor_reset_source->load(std::memory_order_acquire) == anchor_expected_generation ? JNI_TRUE : JNI_FALSE;
    }
    jboolean JNICALL totem_permitted(JNIEnv*, jobject, jlong generation) {
        return permitted() && totem_reset_source && static_cast<std::uint64_t>(generation) == totem_expected_generation &&
            totem_reset_source->load(std::memory_order_acquire) == totem_expected_generation ? JNI_TRUE : JNI_FALSE;
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
    jlong JNICALL aura_service(JNIEnv* env, jobject self, jint operation, jobject entity, jint id, jlong generation, jlong epoch) {
        if (operation == 2) { services::combat::release_crystal_aura(); return 1; }
        if (!permitted() || !aura_reset_source ||
            static_cast<std::uint64_t>(generation) != aura_expected_generation ||
            aura_reset_source->load(std::memory_order_acquire) != aura_expected_generation) return 0;
        try {
            switch (operation) {
            case 0: {
                maps::Entity target(entity, false);
                return target && !friends::is_friend(target) && !services::anti_bot::is_bot(target) &&
                    !platform::jni_context::clear_exception();
            }
            case 1: return services::combat::claim_crystal_aura(id, static_cast<std::uint64_t>(epoch));
            case 3:
                // Serialize the Java validation and action against Triggerbot
                // and other input owners. Every Minecraft read/write remains in Java.
                return services::combat::try_crystal_aura_action(id, static_cast<std::uint64_t>(epoch), [&] {
                    return env->CallBooleanMethod(self, aura_action_id) == JNI_TRUE && !env->ExceptionCheck();
                });
            case 4: return 1;
            case 5: {
                maps::Entity player(entity,false);
                return player && friends::is_friend(player) && !platform::jni_context::clear_exception();
            }
            default: return 0;
            }
        } catch (...) {
            platform::jni_context::clear_exception();
            auto exception = env->FindClass("java/lang/IllegalStateException");
            if (exception) env->ThrowNew(exception, "Crystal Aura shared service failed");
            return 0;
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
#if ONESTEP_MINECRAFT_26_1
            {"net/minecraft/class_1304", "net/minecraft/world/entity/EquipmentSlot"},
            {"net/minecraft/class_1293", "net/minecraft/world/effect/MobEffectInstance"},
            {"net/minecraft/class_5134", "net/minecraft/world/entity/ai/attributes/Attributes"},
            {"net/minecraft/class_9304", "net/minecraft/world/item/enchantment/ItemEnchantments"},
            {"net/minecraft/class_1267", "net/minecraft/world/Difficulty"},
            {"field_5810", "OFF_HAND"},
            {"method_6096", "getArmorValue"},
            {"method_45325", "getAttributeValue"},
            {"method_6067", "getAbsorptionAmount"},
            {"method_6112", "getEffect"},
            {"method_5578", "getAmplifier"},
            {"field_23725", "ARMOR_TOUGHNESS"},
            {"field_5907", "RESISTANCE"},
            {"field_5911", "WEAKNESS"},
            {"field_5910", "STRENGTH"},
            {"method_6118", "getItemBySlot"},
            {"field_6166", "FEET"},
            {"field_6172", "LEGS"},
            {"field_6174", "CHEST"},
            {"field_6169", "HEAD"},
            {"method_58657", "getEnchantments"},
            {"method_57539", "entrySet"},
            {"method_55840", "getRegisteredName"},
            {"method_8407", "getDifficulty"},
            {"method_5461", "getId"},
            {"field_7480", "invulnerable"},
#else
            {"net/minecraft/class_1304", "net/minecraft/class_1304"},
            {"net/minecraft/class_1293", "net/minecraft/class_1293"},
            {"net/minecraft/class_5134", "net/minecraft/class_5134"},
            {"net/minecraft/class_9304", "net/minecraft/class_9304"},
            {"net/minecraft/class_1267", "net/minecraft/class_1267"},
            {"field_5810", "field_5810"},
            {"method_6096", "method_6096"},
            {"method_45325", "method_45325"},
            {"method_6067", "method_6067"},
            {"method_6112", "method_6112"},
            {"method_5578", "method_5578"},
            {"field_23725", "field_23725"},
            {"field_5907", "field_5907"},
            {"field_5911", "field_5911"},
            {"field_5910", "field_5910"},
            {"method_6118", "method_6118"},
            {"field_6166", "field_6166"},
            {"field_6172", "field_6172"},
            {"field_6174", "field_6174"},
            {"field_6169", "field_6169"},
            {"method_58657", "method_58657"},
            {"method_57539", "method_57539"},
            {"method_55840", "method_55840"},
            {"method_8407", "method_8407"},
            {"method_5461", "method_5461"},
            {"field_7480", "field_7480"},
#endif
            {"field_5965", ONESTEP_MC_NAME("field_5965")},
            {"field_8281", ONESTEP_MC_NAME("field_8281")},
            {"method_7325", ONESTEP_MC_NAME("method_7325")},
            {"method_26225", ONESTEP_MC_NAME("method_26225")},
            {"method_5745", ONESTEP_MC_NAME("method_5745")},
            {"method_7350", ONESTEP_MC_NAME("method_7350")},
#if ONESTEP_MINECRAFT_26_1
            {"net/minecraft/class_9323", "net/minecraft/core/component/DataComponentMap"},
            {"net/minecraft/class_1269$class_9860", "net/minecraft/world/InteractionResult$Success"},
            {"net/minecraft/class_1269$class_9861", "net/minecraft/world/InteractionResult$SwingSource"},
            {"comp_2909", "swingSource"},
            {"field_52427", "CLIENT"},
            {"method_45474", "canBeReplaced"},
            {"method_29504", "isDeadOrDying"},
            {"method_57347", "components"},
            {"method_57832", "has"},
#else
            {"net/minecraft/class_9323", "net/minecraft/class_9323"},
            {"net/minecraft/class_1269$class_9860", "net/minecraft/class_1269$class_9860"},
            {"net/minecraft/class_1269$class_9861", "net/minecraft/class_1269$class_9861"},
            {"comp_2909", "comp_2909"},
            {"field_52427", "field_52427"},
            {"method_45474", "method_45474"},
            {"method_29504", "method_29504"},
            {"method_57347", "method_57347"},
            {"method_57832", "method_57832"},
#endif
#if ONESTEP_MINECRAFT_26_1
            {"method_2383", "slotClicked"},
            {"method_25402", "mouseClicked"},
#else
            {"method_2383", "method_2383"},
            {"method_25402", "method_25402"},
#endif
            {"net/minecraft/class_465", ONESTEP_MC_NAME("net/minecraft/class_465")},
            {"field_2797", ONESTEP_MC_NAME("field_2797")},
            {"net/minecraft/class_490", ONESTEP_MC_NAME("net/minecraft/class_490")},
            {"net/minecraft/class_1703", ONESTEP_MC_NAME("net/minecraft/class_1703")},
            {"net/minecraft/class_1713", ONESTEP_MC_NAME("net/minecraft/class_1713")},
            {"field_8288", ONESTEP_MC_NAME("field_8288")},
            {"field_7791", ONESTEP_MC_NAME("field_7791")},
            {"field_7512", ONESTEP_MC_NAME("field_7512")},
            {"field_7763", ONESTEP_MC_NAME("field_7763")},
            {"method_34255", ONESTEP_MC_NAME("method_34255")},
            {"method_2906", ONESTEP_MC_NAME("method_2906")},
            {"method_6032", ONESTEP_MC_NAME("method_6032")},
            {"method_7876", ONESTEP_MC_NAME("method_7876")},
#if ONESTEP_MINECRAFT_26_1
            {"net/minecraft/class_1723", "net/minecraft/world/inventory/InventoryMenu"},
            {"field_7498", "inventoryMenu"},
            {"method_1507", "setScreen"},
            {"method_25419", "onClose"},
            {"field_22789", "width"},
            {"field_22790", "height"},
#else
            {"net/minecraft/class_1723", "net/minecraft/class_1723"},
            {"field_7498", "field_7498"},
            {"method_1507", "method_1507"},
            {"method_25419", "method_25419"},
            {"field_22789", "field_22789"},
            {"field_22790", "field_22790"},
#endif
            {"method_23666", "method_23666"},

            {"net/minecraft/class_1819", ONESTEP_MC_NAME("net/minecraft/class_1819")},
            {"net/minecraft/class_9331", ONESTEP_MC_NAME("net/minecraft/class_9331")},
            {"net/minecraft/class_9334", ONESTEP_MC_NAME("net/minecraft/class_9334")},
            {"field_50075", ONESTEP_MC_NAME("field_50075")},
            {"method_6047", ONESTEP_MC_NAME("method_6047")},
            {"method_6079", ONESTEP_MC_NAME("method_6079")},
            {"method_57824", ONESTEP_MC_NAME("method_57824")},
            {"method_58694", ONESTEP_MC_NAME("method_58694")},

            {"net/minecraft/class_4969", ONESTEP_MC_NAME("net/minecraft/class_4969")},
            {"net/minecraft/class_2758", ONESTEP_MC_NAME("net/minecraft/class_2758")},
            {"net/minecraft/class_2769", ONESTEP_MC_NAME("net/minecraft/class_2769")},
            {"field_23141", ONESTEP_MC_NAME("field_23141")},
            {"field_8801", ONESTEP_MC_NAME("field_8801")},
            {"field_23152", ONESTEP_MC_NAME("field_23152")},
            {"field_23153", ONESTEP_MC_NAME("field_23153")},
            {"field_11033", ONESTEP_MC_NAME("field_11033")},
            {"field_11043", ONESTEP_MC_NAME("field_11043")},
            {"field_11035", ONESTEP_MC_NAME("field_11035")},
            {"field_11039", ONESTEP_MC_NAME("field_11039")},
            {"field_11034", ONESTEP_MC_NAME("field_11034")},
            {"field_1788", ONESTEP_MC_NAME("field_1788")},
            {"method_11654", ONESTEP_MC_NAME("method_11654")},
            {"method_17780", ONESTEP_MC_NAME("method_17780")},
            {"method_10148", ONESTEP_MC_NAME("method_10148")},
            {"method_10164", ONESTEP_MC_NAME("method_10164")},
            {"method_10165", ONESTEP_MC_NAME("method_10165")},
            {"method_18456", ONESTEP_MC_NAME("method_18456")},

            {"net/minecraft/class_1656", ONESTEP_MC_NAME("net/minecraft/class_1656")},
            {"net/minecraft/class_1282", ONESTEP_MC_NAME("net/minecraft/class_1282")},
            {"field_1903", ONESTEP_MC_NAME("field_1903")},
            {"field_6235", ONESTEP_MC_NAME("field_6235")},
            {"field_6012", ONESTEP_MC_NAME("field_6012")},
            {"field_6228", ONESTEP_MC_NAME("field_6228")},
            {"field_7478", ONESTEP_MC_NAME("field_7478")},
            {"field_7479", ONESTEP_MC_NAME("field_7479")},
            {"method_6113", ONESTEP_MC_NAME("method_6113")},
            {"method_31549", ONESTEP_MC_NAME("method_31549")},
            {"method_6081", ONESTEP_MC_NAME("method_6081")},
            {"method_5529", ONESTEP_MC_NAME("method_5529")},
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
                {const_cast<char*>("safeAnchorNativeTick"), const_cast<char*>("()V"), reinterpret_cast<void*>(&safe_anchor_native_tick)},
                {const_cast<char*>("auraService"), const_cast<char*>("(ILjava/lang/Object;IJJ)J"), reinterpret_cast<void*>(&aura_service)},
                {const_cast<char*>("triggerService"), const_cast<char*>("(ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;IJ)J"), reinterpret_cast<void*>(&trigger_service)},
                {const_cast<char*>("jumpPermitted"), const_cast<char*>("(J)Z"), reinterpret_cast<void*>(&jump_permitted)},
                {const_cast<char*>("anchorPermitted"), const_cast<char*>("(J)Z"), reinterpret_cast<void*>(&anchor_permitted)},
                {const_cast<char*>("totemPermitted"), const_cast<char*>("(J)Z"), reinterpret_cast<void*>(&totem_permitted)}
            };
            if (env->RegisterNatives(type, callbacks, static_cast<jint>(std::size(callbacks))) != JNI_OK || env->ExceptionCheck()) throw std::runtime_error("World native service binding");
            set_safe_anchor_active_id = checked(env, env->GetMethodID(type, "setSafeAnchorActive", "(Z)V"));
            is_safe_anchor_active_id = checked(env, env->GetMethodID(type, "isSafeAnchorActive", "()Z"));
            totem_tick_id = checked(env,env->GetMethodID(type,"tickTotem","(ZJ[IZ)V"));
            totem_cancel_id = checked(env,env->GetMethodID(type,"cancelTotem","()V"));
            anchor_install_events_id = checked(env,env->GetMethodID(type,"installAnchorEvents","()V"));
            anchor_tick_id = checked(env, env->GetMethodID(type, "tickAnchor", "(ZJI[Z[I)V"));
            anchor_cancel_id = checked(env, env->GetMethodID(type, "cancelAnchor", "()V"));
            jump_tick_id = checked(env, env->GetMethodID(type, "tickJump", "(ZJIIIZ)V"));
            jump_cancel_id = checked(env, env->GetMethodID(type, "cancelJump", "()V"));
            trigger_tick_id = checked(env, env->GetMethodID(type, "tickTrigger", "(ZJJFFIIIIIZZZ)V"));
            trigger_cancel_id = checked(env, env->GetMethodID(type, "cancelTrigger", "()V"));
            aura_tick_id = checked(env,env->GetMethodID(type,"tickAura","(ZJ[I)[I"));
            aura_cancel_id = checked(env,env->GetMethodID(type,"cancelAura","()V"));
            aura_action_id = checked(env,env->GetMethodID(type,"executeAuraAction","()Z"));
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
            report_java_failure(env,"initialization");
            platform::jni_context::clear_exception(); failed = true;
            OutputDebugStringW(L"[OneStep] World bridge setup unavailable; Java-only combat controllers inactive.\n"); return false;
        }
    }
    services::game::hybrid::result prepare(unsigned feature) {
        using services::game::hybrid::result;

        if (feature < 1 || feature > 8) return result::unavailable;
        if constexpr (mc::target.build_id != 12111) if (feature < 3) return result::native_fallback;
        if (!permitted() || !platform::jni_context::bind_current_thread() || suspended[feature]) return result::unavailable;
        if (rejected[feature] || !connect(jni::get_env())) return result::native_fallback;
        if (!prepared[feature]) {
            jni::get_env()->CallVoidMethod(bridge, prepare_id, static_cast<jint>(feature));
            if (jni::get_env()->ExceptionCheck()) {
                report_java_failure(jni::get_env(),"feature preparation");
                platform::jni_context::clear_exception(); rejected[feature] = true; return result::native_fallback;
            }
            prepared[feature] = true;
        }
        return result::ready;
    }
    bool checked_result(unsigned feature, bool value) {
        auto env=jni::get_env();
        if (!env) return false;
        if (!env->ExceptionCheck()) return value;
        report_java_failure(env,"runtime operation");
        platform::jni_context::clear_exception();
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

    void JNICALL safe_anchor_native_tick(JNIEnv* env, jobject self)
    {
        (void)env; (void)self;
        modules::safe_anchor::client_tick();
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
    if (!active || !permitted()) { cancel_crystal(); return; }
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
void services::game::world_bridge::tick_jump(const jump_reset_config::config& settings, bool active,
    const std::atomic<std::uint64_t>& generation)
{
    if (!active || !permitted()) { cancel_jump(); return; }
    if (!platform::jni_context::bind_current_thread()) return;
    jni::frame frame{128};
    if (prepare(5) != hybrid::result::ready) { cancel_jump(); return; }
    jump_expected_generation = generation.load(std::memory_order_acquire);
    jump_reset_source = &generation;
    jni::get_env()->CallVoidMethod(bridge, jump_tick_id, JNI_TRUE,
        static_cast<jlong>(jump_expected_generation), static_cast<jint>(settings.mode),
        static_cast<jint>(settings.chance), static_cast<jint>(settings.jump_delay_ms), static_cast<jboolean>(settings.only_combat));
    if (!checked_result(5, true)) cancel_jump();
}
void services::game::world_bridge::cancel_jump()
{
    jump_reset_source = nullptr; jump_expected_generation = 0;
    if (!bridge || !jump_cancel_id || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{96};
    jni::get_env()->CallVoidMethod(bridge, jump_cancel_id);
    if (platform::jni_context::clear_exception()) suspended[5] = true;
}
void services::game::world_bridge::tick_anchor(const module_settings::anchor_macro& settings, bool active,
    const std::atomic<std::uint64_t>& generation, std::uint64_t expected_generation)
{
    if (!active || !permitted() || generation.load(std::memory_order_acquire) != expected_generation) { cancel_anchor(); return; }
    if (!platform::jni_context::bind_current_thread()) return;
    jni::frame frame{128};
    if (prepare(6) != hybrid::result::ready) { cancel_anchor(); return; }
    if (settings.execution_mode == 1 && !install_anchor_hooks(jni::get_env())) { cancel_anchor(); return; }
    anchor_expected_generation = expected_generation;
    anchor_reset_source = &generation;
    auto env = jni::get_env();
    auto flags = env->NewBooleanArray(5);
    const bool flags_error = platform::jni_context::clear_exception();
    if (!flags || flags_error) { cancel_anchor(); return; }
    auto values = env->NewIntArray(8);
    const bool values_error = platform::jni_context::clear_exception();
    if (!values || values_error) { cancel_anchor(); return; }
    const jboolean switches[]{static_cast<jboolean>(settings.while_use),static_cast<jboolean>(settings.stop_on_kill),
        static_cast<jboolean>(settings.click_simulation),static_cast<jboolean>(settings.only_own),static_cast<jboolean>(settings.only_charge)};
    const jint numbers[]{settings.place_chance,settings.switch_delay,settings.switch_chance,settings.glowstone_delay,
        settings.glowstone_chance,settings.explode_delay,settings.explode_chance,settings.explode_slot};
    env->SetBooleanArrayRegion(flags,0,5,switches); env->SetIntArrayRegion(values,0,8,numbers);
    if (platform::jni_context::clear_exception()) { cancel_anchor(); return; }
    env->CallVoidMethod(bridge,anchor_tick_id,JNI_TRUE,static_cast<jlong>(anchor_expected_generation),static_cast<jint>(settings.execution_mode),flags,values);
    if (!checked_result(6,true)) cancel_anchor();
}
void services::game::world_bridge::cancel_anchor()
{
    anchor_reset_source = nullptr; anchor_expected_generation = 0;
    if (!bridge || !anchor_cancel_id || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{96};
    jni::get_env()->CallVoidMethod(bridge,anchor_cancel_id);
    if (platform::jni_context::clear_exception()) suspended[6] = true;
}
void services::game::world_bridge::tick_totem(const module_settings::auto_totem& s, bool active,
    const std::atomic<std::uint64_t>& generation, std::uint64_t expected_generation)
{
    if (!active || !permitted() || generation.load(std::memory_order_acquire) != expected_generation) { cancel_totem(); return; }
    if (!platform::jni_context::bind_current_thread()) return;
    jni::frame frame{64};
    if (!connect(jni::get_env()) || !install_anchor_hooks(jni::get_env()) || prepare(7) != hybrid::result::ready) { cancel_totem(); return; }
    totem_reset_source = &generation; totem_expected_generation = expected_generation;
    auto env = jni::get_env();
    auto values = env->NewIntArray(7);
    if (platform::jni_context::clear_exception() || !values) { cancel_totem(); return; }
    const jint settings[]{s.mode,s.delay,static_cast<jint>(s.hotbar),s.totem_slot,
        static_cast<jint>(s.auto_switch),static_cast<jint>(s.force_totem),s.stay_open_for};
    env->SetIntArrayRegion(values,0,7,settings);
    if (platform::jni_context::clear_exception()) { cancel_totem(); return; }
    env->CallVoidMethod(bridge,totem_tick_id,JNI_TRUE,static_cast<jlong>(expected_generation),values,static_cast<jboolean>(s.auto_open));
    if (!checked_result(7,true)) cancel_totem();
}
void services::game::world_bridge::cancel_totem()
{
    totem_reset_source=nullptr; totem_expected_generation=0;
    if (!bridge || !totem_cancel_id || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{32};
    jni::get_env()->CallVoidMethod(bridge,totem_cancel_id);
    if (platform::jni_context::clear_exception()) suspended[7]=true;
}

std::array<int,3> services::game::world_bridge::tick_aura(const module_settings::crystal_aura& settings, bool active,
    const std::atomic<std::uint64_t>& generation, std::uint64_t expected_generation)
{
    std::array<int,3> status{-1,0,0};
    if (!active || !permitted() ||
        generation.load(std::memory_order_acquire) != expected_generation) { cancel_aura(); return status; }
    if (!platform::jni_context::bind_current_thread()) { services::combat::release_crystal_aura(); return status; }
    jni::frame frame{64};
    if (prepare(8) != hybrid::result::ready) { cancel_aura(); status[2]=4; return status; }
    aura_reset_source=&generation; aura_expected_generation=expected_generation;
    auto env=jni::get_env();
    auto values=env->NewIntArray(11);
    if (!values || env->ExceptionCheck()) { checked_result(8,false); cancel_aura(); return status; }
    const jint config[]{settings.target_mode,settings.switch_delay_ms,settings.confirm_timeout_ms,
        settings.place_delay_ms,settings.break_delay_ms,
        static_cast<jint>(std::lround(settings.target_range*1000)),static_cast<jint>(std::lround(settings.place_range*1000)),
        static_cast<jint>(std::lround(settings.break_range*1000)),static_cast<jint>(std::lround(settings.min_damage*1000)),
        static_cast<jint>(std::lround(settings.max_self_damage*1000)),
        (settings.place_enabled?1:0)|(settings.break_enabled?2:0)|(settings.auto_switch?4:0)|
        (settings.restore_slot?8:0)|(settings.support?16:0)|(settings.damage_check?32:0)};
    env->SetIntArrayRegion(values,0,11,config);
    if (!checked_result(8,true)) { cancel_aura(); return status; }
    auto result=static_cast<jintArray>(env->CallObjectMethod(bridge,aura_tick_id,JNI_TRUE,
        static_cast<jlong>(expected_generation),values));
    if (!checked_result(8,result!=nullptr) || !result) { cancel_aura(); status[2]=4; return status; }
    if (env->GetArrayLength(result)!=3) { suspended[8]=true; cancel_aura(); status[2]=4; return status; }
    std::array<jint,3> raw{}; env->GetIntArrayRegion(result,0,3,raw.data());
    if (!checked_result(8,true)) { cancel_aura(); status[2]=4; return status; }
    for (unsigned i=0;i<3;++i) status[i]=raw[i];
    return status;
}
void services::game::world_bridge::cancel_aura()
{
    aura_reset_source=nullptr; aura_expected_generation=0;
    services::combat::release_crystal_aura();
    if (!bridge || !aura_cancel_id || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{32};
    jni::get_env()->CallVoidMethod(bridge,aura_cancel_id);
    if (platform::jni_context::clear_exception()) suspended[8]=true;
}


void services::game::world_bridge::set_safe_anchor_active(bool active)
{
    safe_anchor_active_state.store(active, std::memory_order_release);
    if (!bridge || !set_safe_anchor_active_id || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{32};
    jni::get_env()->CallVoidMethod(bridge, set_safe_anchor_active_id, active ? JNI_TRUE : JNI_FALSE);
    platform::jni_context::clear_exception();
}

bool services::game::world_bridge::is_safe_anchor_ticking()
{
    if (!bridge || !is_safe_anchor_active_id || !platform::jni_context::bind_current_thread()) return false;
    jni::frame frame{32};
    jboolean result = jni::get_env()->CallBooleanMethod(bridge, is_safe_anchor_active_id);
    return !platform::jni_context::clear_exception() && result == JNI_TRUE;
}

void services::game::world_bridge::shutdown()
{
    set_safe_anchor_active_id = is_safe_anchor_active_id = nullptr;
    safe_anchor_active_state.store(false);
    cancel_aura();
    totem_reset_source = nullptr; totem_expected_generation = 0;
    anchor_reset_source = nullptr; anchor_expected_generation = 0;
    jump_reset_source = nullptr; jump_expected_generation = 0;
    if (bridge && platform::jni_context::bind_current_thread()) {
        jni::get_env()->CallVoidMethod(bridge, stop_id); platform::jni_context::clear_exception();
        shutdown_anchor_hooks(jni::get_env());
        auto type = jni::get_env()->GetObjectClass(bridge);
        if (type) { jni::get_env()->UnregisterNatives(type); jni::get_env()->DeleteLocalRef(type); }
        platform::jni_context::clear_exception(); jni::get_env()->DeleteGlobalRef(bridge);
    }
    bridge = nullptr; prepare_id = edge_id = support_id = force_id = input_id = release_edge_id = nullptr;
    pearl_id = begin_id = use_id = release_pearl_id = stop_id = nullptr;
    crystal_tick_id = crystal_cancel_id = crystal_input_id = nullptr;
    trigger_tick_id = trigger_cancel_id = nullptr; trigger_reset_source = nullptr; trigger_helper = 0;
    aura_tick_id = aura_cancel_id = aura_action_id = nullptr;
    jump_tick_id = jump_cancel_id = nullptr;
    anchor_tick_id = anchor_cancel_id = anchor_install_events_id = nullptr;
    totem_tick_id = totem_cancel_id = nullptr;
    anchor_hooks_failed=false;
    services::combat::release_trigger();
    prepared = {}; rejected = {}; suspended = {}; failed = false;
}
