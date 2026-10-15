#include "java_hook.hpp"
#include "../platform/jni/jni_context.hpp"
#include "../meta_jni.hpp"
#include "../../resource.h"
#include <jvmti.h>
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <vector>

namespace
{
    jvmtiEnv* environment = nullptr;
    jclass bridge = nullptr;
    jobject hook_client = nullptr;
    jmethodID transform = nullptr;
    std::vector<jclass> transformed_classes;
    std::atomic<java_hook::callback> consumer{nullptr};
    std::atomic_bool ready{false};
    bool attempted = false;
    std::atomic_bool restoring{false}, transform_failed{false};
    std::mutex transform_mutex;
    std::mutex install_mutex;
    std::string failure;
    constexpr const char* names[]{"net/minecraft/class_746","net/minecraft/class_636",
        "net/minecraft/class_634","net/minecraft/class_8673","net/minecraft/class_757"};
    bool exception(JNIEnv* env)
    {
        if (!env->ExceptionCheck()) return false;
        env->ExceptionClear(); return true;
    }
    jboolean JNICALL native_event(JNIEnv* env, jclass, jint kind, jobject self, jobject a, jobject b)
    {
        const auto callback = consumer.load(std::memory_order_acquire);
        if (!ready.load(std::memory_order_acquire) || !callback || env->ExceptionCheck()) return JNI_FALSE;
        try {
            const bool cancel = callback(env, kind, self, a, b);
            if (exception(env)) return JNI_FALSE;
            return cancel ? JNI_TRUE : JNI_FALSE;
        } catch (...) { exception(env); return JNI_FALSE; }
    }
    void JNICALL class_load(jvmtiEnv* ti, JNIEnv* env, jclass, jobject, const char* name,
        jobject, jint size, const unsigned char* bytes, jint* new_size, unsigned char** new_bytes)
    {
        if (restoring.load() || !name) return;
        bool selected = false;
        for (const auto* target : names) if (std::string_view(name) == target) selected = true;
        if (!selected) return;
        std::lock_guard transform_lock(transform_mutex);
        if (restoring.load() || !bridge || !transform) return;
        if (env->PushLocalFrame(16) != JNI_OK) { exception(env); transform_failed = true; return; }
        auto input = env->NewByteArray(size);
        auto class_name = env->NewStringUTF(name);
        if (input) env->SetByteArrayRegion(input, 0, size, reinterpret_cast<const jbyte*>(bytes));
        auto output = input ? static_cast<jbyteArray>(env->CallStaticObjectMethod(bridge, transform, class_name, input)) : nullptr;
        if (exception(env) || !output) { transform_failed = true; env->PopLocalFrame(nullptr); return; }
        const jint length = env->GetArrayLength(output);
        unsigned char* allocated = nullptr;
        if (length <= 0 || ti->Allocate(length, &allocated) != JVMTI_ERROR_NONE) transform_failed = true;
        else {
            env->GetByteArrayRegion(output, 0, length, reinterpret_cast<jbyte*>(allocated));
            if (exception(env)) { ti->Deallocate(allocated); transform_failed = true; }
            else { *new_size = length; *new_bytes = allocated; }
        }
        env->PopLocalFrame(nullptr);
    }
    jclass load(JNIEnv* env, jobject loader, const char* name)
    {
        auto type = env->GetObjectClass(loader);
        auto method = env->GetMethodID(type, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        if (!method || exception(env)) return nullptr;
        std::string dotted(name);
        for (auto& c : dotted) if (c == '/') c = '.';
        auto text = env->NewStringUTF(dotted.c_str());
        auto result = static_cast<jclass>(env->CallObjectMethod(loader, method, text));
        return exception(env) ? nullptr : result;
    }
    jclass define_resource(JNIEnv* env, jobject loader, int id, const char* name)
    {
        if (auto existing = load(env, loader, name)) return existing;
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&define_resource), &module);
        const auto resource = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
        if (!resource) return nullptr;
        const auto memory = LoadResource(module, resource);
        const auto data = static_cast<const jbyte*>(LockResource(memory));
        return data ? env->DefineClass(name, loader, data, static_cast<jsize>(SizeofResource(module, resource))) : nullptr;
    }
    void deactivate(JNIEnv* env)
    {
        if (bridge) {
            auto method = env->GetStaticMethodID(bridge, "deactivate", "()V");
            if (method) env->CallStaticVoidMethod(bridge, method);
            exception(env);
        }
        ready.store(false, std::memory_order_release);
        consumer.store(nullptr, std::memory_order_release);
    }
    void release(JNIEnv* env)
    {
        deactivate(env);
        if (environment) {
            restoring = true;
            environment->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
            { std::lock_guard transform_lock(transform_mutex); }
            if (!transformed_classes.empty())
                environment->RetransformClasses(static_cast<jint>(transformed_classes.size()), transformed_classes.data());
            jvmtiEventCallbacks empty{};
            environment->SetEventCallbacks(&empty, sizeof empty);
            environment->DisposeEnvironment(); environment = nullptr;
        }
        for (auto type : transformed_classes) env->DeleteGlobalRef(type);
        transformed_classes.clear();
        // Even a denied restore leaves dispatch safely inactive before native unload.
        if (bridge) { env->UnregisterNatives(bridge); exception(env); env->DeleteGlobalRef(bridge); bridge = nullptr; }
        if (hook_client) { env->DeleteGlobalRef(hook_client); hook_client = nullptr; }
        transform = nullptr;
    }
}
bool java_hook::init() { return true; }
bool java_hook::ensure_vape(JNIEnv* env, jobject client, callback handler)
{
    std::lock_guard lock(install_mutex);
    if (ready.load()) return true;
    if (attempted || !env || !client) return false;
    attempted = true;
    auto fail = [&](const char* reason) { failure = reason; release(env); return false; };
    if (env->PushLocalFrame(64) != JNI_OK) { exception(env); return fail("Vape: JNI local frame unavailable."); }
    struct local_frame { JNIEnv* env; ~local_frame() { env->PopLocalFrame(nullptr); } } frame{env};
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) != JNI_OK || vm->GetEnv(reinterpret_cast<void**>(&environment), JVMTI_VERSION_1_2) != JNI_OK)
        return fail("Vape: JVM class transformation unavailable.");
    jvmtiCapabilities potential{};
    if (environment->GetPotentialCapabilities(&potential) != JVMTI_ERROR_NONE || !potential.can_retransform_classes)
        return fail("Vape: live retransformation is not supported by this JVM.");
    jvmtiCapabilities requested{}; requested.can_retransform_classes = 1;
    if (environment->AddCapabilities(&requested) != JVMTI_ERROR_NONE)
        return fail("Vape: retransformation capability denied.");
    auto client_class = env->GetObjectClass(client);
    auto class_type = env->FindClass("java/lang/Class");
    auto get_loader = env->GetMethodID(class_type, "getClassLoader", "()Ljava/lang/ClassLoader;");
    auto loader = env->CallObjectMethod(client_class, get_loader);
    if (exception(env) || !loader) return fail("Vape: Minecraft class loader unavailable.");
    auto local_bridge = define_resource(env, loader, IDR_VAPE_BRIDGE, "wolf/vape/NativeBridge");
    if (!local_bridge || exception(env)) return fail("Vape: Java bridge could not be defined.");
    bridge = static_cast<jclass>(env->NewGlobalRef(local_bridge));
    hook_client = env->NewGlobalRef(client);
    auto abi = env->GetStaticFieldID(bridge, "ABI_VERSION", "I");
    const bool abi_error=exception(env);
    if (!abi || abi_error || env->GetStaticIntField(bridge,abi) != 2)
        return fail("Vape: old Java bridge is still loaded; restart Minecraft before loading this DLL.");
    if (!define_resource(env, loader, IDR_VAPE_WRITER, "wolf/vape/NativeBridge$Writer") || exception(env))
        return fail("Vape: ASM bridge dependency unavailable.");
    const JNINativeMethod natives[]{ { const_cast<char*>("event"),
        const_cast<char*>("(ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)Z"), reinterpret_cast<void*>(&native_event) } };
    if (env->RegisterNatives(bridge, natives, 1) != JNI_OK || exception(env))
        return fail("Vape: native callback registration failed.");
    auto warmup = env->GetStaticMethodID(bridge, "warmup", "()V");
    transform = env->GetStaticMethodID(bridge, "transform", "(Ljava/lang/String;[B)[B");
    auto mask = env->GetStaticFieldID(bridge, "installedMask", "I");
    if (!warmup || !transform || !mask || exception(env)) return fail("Vape: bridge ABI mismatch.");
    env->CallStaticVoidMethod(bridge, warmup);
    if (exception(env)) return fail("Vape: Fabric ASM is unavailable or incompatible.");
    env->SetStaticIntField(bridge, mask, 0);
    for (const auto* name : names) {
        auto type = load(env, loader, name);
        jboolean modifiable = JNI_FALSE;
        if (!type || environment->IsModifiableClass(type, &modifiable) != JVMTI_ERROR_NONE || !modifiable)
            return fail("Vape: a required Minecraft class cannot be transformed.");
        transformed_classes.push_back(static_cast<jclass>(env->NewGlobalRef(type)));
    }
    jvmtiEventCallbacks callbacks{}; callbacks.ClassFileLoadHook = &class_load;
    restoring = false; transform_failed = false;
    if (environment->SetEventCallbacks(&callbacks, sizeof callbacks) != JVMTI_ERROR_NONE ||
        environment->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr) != JVMTI_ERROR_NONE)
        return fail("Vape: class transform callback denied.");
    if (environment->RetransformClasses(static_cast<jint>(transformed_classes.size()), transformed_classes.data()) != JVMTI_ERROR_NONE ||
        transform_failed || env->GetStaticIntField(bridge, mask) != 63)
        return fail("Vape: exact Fabric 1.21.1 hooks did not match; no partial hooks activated.");
    consumer.store(handler, std::memory_order_release); ready.store(true, std::memory_order_release);
    env->CallStaticVoidMethod(bridge, env->GetStaticMethodID(bridge, "activate", "()V"));
    if (exception(env)) return fail("Vape: callback activation failed.");
    failure.clear(); return true;
}
bool java_hook::vape_ready() { return ready.load(std::memory_order_acquire); }
std::string java_hook::vape_error() { std::lock_guard lock(install_mutex); return failure; }
void java_hook::shutdown()
{
    if (!platform::jni_context::bind_current_thread()) return;
    auto* env = jni::get_env();
    // Never hold install_mutex while waiting for the client thread: its options
    // panel may be reading vape_error() under the same mutex.
    if (ready.load() && bridge && hook_client) {
        auto quiesce = env->GetStaticMethodID(bridge, "quiesce", "(Ljava/lang/Object;)V");
        if (quiesce) env->CallStaticVoidMethod(bridge, quiesce, hook_client);
        exception(env);
    }
    std::lock_guard lock(install_mutex);
    release(env);
}
