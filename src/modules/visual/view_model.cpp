#include "view_model.hpp"
#include "view_model_presets.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/client_context.hpp"
#include "../../../resource.h"
#include <jvmti.h>
#include <array>
#include <cstring>
#include <span>
#include <stdexcept>

extern JavaVM* jvm;
namespace
{
    template<class T> T vm_checked(JNIEnv* env,T value)
    {
        if (env->ExceptionCheck() || !value) throw std::runtime_error("View Model JNI binding");
        return value;
    }
    std::span<const unsigned char> vm_resource(int id)
    {
        HMODULE owner=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&vm_resource),&owner);
        auto entry=FindResourceW(owner,MAKEINTRESOURCEW(id),RT_RCDATA);
        if (!entry) return {};
        return {static_cast<const unsigned char*>(LockResource(LoadResource(owner,entry))),SizeofResource(owner,entry)};
    }
}
struct modules::view_model::runtime
{
    inline static std::atomic<runtime*> transforming{nullptr};
    jvmtiEnv* tooling=nullptr;
    jclass renderer=nullptr,bridge=nullptr,transformer=nullptr;
    jmethodID configure=nullptr,transform=nullptr,stop=nullptr,healthy=nullptr;
    bool installed=false,changed=false,failed=false,configured=false,was_active=false;
    module_settings::view_model previous{};
    std::string error;
    static void JNICALL class_bytes(jvmtiEnv* tools,JNIEnv* env,jclass redefining,jobject,
        const char*,jobject,jint size,const unsigned char* bytes,jint* new_size,unsigned char** new_bytes)
    {
        auto* self=transforming.load(std::memory_order_acquire);
        if (!self || !redefining || !env->IsSameObject(redefining,self->renderer)) return;
        if (env->PushLocalFrame(8)!=JNI_OK) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
        auto input=env->NewByteArray(size);
        if (input) env->SetByteArrayRegion(input,0,size,reinterpret_cast<const jbyte*>(bytes));
        auto result=input && !env->ExceptionCheck()
            ? static_cast<jbyteArray>(env->CallStaticObjectMethod(self->transformer,self->transform,input,static_cast<jint>(mc::target.build_id))) : nullptr;
        if (result && !env->ExceptionCheck()) {
            const int length=env->GetArrayLength(result); unsigned char* output=nullptr;
            if (length>0 && length<size*4+65536 && tools->Allocate(length,&output)==JVMTI_ERROR_NONE) {
                env->GetByteArrayRegion(result,0,length,reinterpret_cast<jbyte*>(output));
                if (!env->ExceptionCheck()) { *new_size=length; *new_bytes=output; self->changed=true; }
                else tools->Deallocate(output);
            }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->PopLocalFrame(nullptr);
    }
    void disable(JNIEnv* env)
    {
        if (bridge && stop) env->CallStaticVoidMethod(bridge,stop);
        if (env->ExceptionCheck()) env->ExceptionClear();
        was_active=false; configured=false;
    }
    void shutdown(JNIEnv* env)
    {
        disable(env);
        if (tooling) {
            tooling->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            // Restore only this JVMTI environment's edits, retaining other mods.
            if (changed && renderer) tooling->RetransformClasses(1,&renderer);
            tooling->DisposeEnvironment(); tooling=nullptr;
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        for (auto* ref:{&renderer,&bridge,&transformer}) if (*ref) { env->DeleteGlobalRef(*ref); *ref=nullptr; }
        installed=changed=false; configure=transform=stop=healthy=nullptr;
    }
    bool connect(JNIEnv* env)
    {
        if (installed) return true;
        if (failed || !jvm) return false;
        try {
            auto game=vm_checked(env,jni::get_cached_jclass<maps::MinecraftClient>());
            auto klass=vm_checked(env,env->FindClass("java/lang/Class"));
            auto get_loader=vm_checked(env,env->GetMethodID(klass,"getClassLoader","()Ljava/lang/ClassLoader;"));
            auto loader=vm_checked(env,env->CallObjectMethod(game,get_loader));
            auto for_name=vm_checked(env,env->GetStaticMethodID(klass,"forName","(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;"));
            auto load=[&](const char* dotted,const char* binary,int resource) {
                auto name=vm_checked(env,env->NewStringUTF(dotted));
                auto type=static_cast<jclass>(env->CallStaticObjectMethod(klass,for_name,name,JNI_TRUE,loader));
                if (env->ExceptionCheck()) { env->ExceptionClear(); type=nullptr; }
                if (!type && resource) {
                    auto bytes=vm_resource(resource);
                    if (bytes.empty()) throw std::runtime_error("View Model resource missing");
                    type=env->DefineClass(binary,loader,reinterpret_cast<const jbyte*>(bytes.data()),static_cast<jsize>(bytes.size()));
                }
                return static_cast<jclass>(vm_checked(env,env->NewGlobalRef(vm_checked(env,type))));
            };
            bridge=load("net.onestep.visual.ViewModelBridge","net/onestep/visual/ViewModelBridge",IDR_VIEWMODEL_BRIDGE);
            transformer=load("net.onestep.visual.ViewModelTransformer","net/onestep/visual/ViewModelTransformer",IDR_VIEWMODEL_TRANSFORMER);
            renderer=load(ONESTEP_MINECRAFT_26_1 ? "net.minecraft.client.renderer.ItemInHandRenderer" : "net.minecraft.class_759",nullptr,0);
            configure=vm_checked(env,env->GetStaticMethodID(bridge,"configure","([FZ)V"));
            stop=vm_checked(env,env->GetStaticMethodID(bridge,"stop","()V"));
            healthy=vm_checked(env,env->GetStaticMethodID(bridge,"healthy","()Z"));
            auto prepare=vm_checked(env,env->GetStaticMethodID(bridge,"prepare","(Ljava/lang/ClassLoader;I)V"));
            env->CallStaticVoidMethod(bridge,prepare,loader,static_cast<jint>(mc::target.build_id));
            if (env->ExceptionCheck()) throw std::runtime_error("View Model matrix contract");
            transform=vm_checked(env,env->GetStaticMethodID(transformer,"transform","([BI)[B"));
            if (jvm->GetEnv(reinterpret_cast<void**>(&tooling),JVMTI_VERSION_1_2)!=JNI_OK || !tooling) throw std::runtime_error("JVMTI");
            jvmtiCapabilities capability{}; capability.can_retransform_classes=1;
            if (tooling->AddCapabilities(&capability)!=JVMTI_ERROR_NONE) throw std::runtime_error("Retransformation unavailable");
            jboolean modifiable=JNI_FALSE;
            if (tooling->IsModifiableClass(renderer,&modifiable)!=JVMTI_ERROR_NONE || !modifiable) throw std::runtime_error("Renderer not modifiable");
            jvmtiEventCallbacks callbacks{}; callbacks.ClassFileLoadHook=&class_bytes;
            if (tooling->SetEventCallbacks(&callbacks,sizeof(callbacks))!=JVMTI_ERROR_NONE) throw std::runtime_error("Hook callback");
            transforming.store(this,std::memory_order_release);
            auto status=tooling->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            if (status==JVMTI_ERROR_NONE) status=tooling->RetransformClasses(1,&renderer);
            tooling->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            transforming.store(nullptr,std::memory_order_release);
            if (status!=JVMTI_ERROR_NONE || !changed) throw std::runtime_error("Unexpected held-item renderer");
            installed=true; return true;
        } catch (...) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            transforming.store(nullptr,std::memory_order_release);
            shutdown(env); failed=true; error="View Model could not connect to the held-item renderer."; return false;
        }
    }
    void update(JNIEnv* env,const module_settings::view_model& settings,bool active)
    {
        if (was_active) {
            const bool ready=env->CallStaticBooleanMethod(bridge,healthy)==JNI_TRUE;
            if (!ready || env->ExceptionCheck()) { disable(env); failed=true; error="View Model stopped safely after a render error. Restart the client to retry."; return; }
        }
        if (configured && previous==settings && active==was_active) return;
        const std::array<jfloat,8> values{settings.x,settings.y,settings.z,settings.scale,settings.pitch,settings.yaw,settings.roll,settings.both_hands ? 1.f : 0.f};
        auto array=env->NewFloatArray(static_cast<jsize>(values.size()));
        if (array) env->SetFloatArrayRegion(array,0,static_cast<jsize>(values.size()),values.data());
        if (array && !env->ExceptionCheck()) env->CallStaticVoidMethod(bridge,configure,array,static_cast<jboolean>(active));
        if (!array || env->ExceptionCheck()) { disable(env); failed=true; error="View Model was stopped after a render error."; return; }
        previous=settings; configured=true; was_active=active;
    }
};
modules::view_model::view_model() : module("view model","Visual","view_model"),state_(std::make_unique<runtime>()) {}
modules::view_model::~view_model() = default;
void modules::view_model::on_render_shutdown()
{
    if (platform::jni_context::bind_current_thread()) { jni::frame frame{16}; state_->shutdown(jni::get_env()); }
}
void modules::view_model::render_overlay()
{
    auto& state=*state_;
    if (!enabled && !state.was_active) return;
    if (!platform::jni_context::bind_current_thread()) return;
    jni::frame frame{48}; auto* env=jni::get_env();
    services::game::client_context context;
    const bool active=enabled && !gui::draw && !gui::loader_visible && !gui::should_unload &&
        services::game::capture_client_context(context,true,false) && !context.screen;
    if (platform::jni_context::clear_exception()) { state.disable(env); return; }
    if (!active) { if (state.was_active) state.disable(env); return; }
    if (state.failed) return;
    if (state.connect(env)) state.update(env,settings_.read(),true);
}
void modules::view_model::render_options()
{
    gui::settings::edit(settings_,module_settings::view_model::schema(),[](auto& candidate) {
        static constexpr const char* names[]{"Custom","Vanilla","Compact","PvP","Low","Cinematic"};
        int selected=view_model_presets::selected(candidate)+1;
        if (gui::option_combo("Preset",&selected,names,static_cast<int>(std::size(names))) && selected>0)
            view_model_presets::apply(candidate,selected-1);
    });
    gui::settings::render(settings_,module_settings::view_model::schema());
    if (!state_->error.empty()) { ImGui::PushTextWrapPos(); ImGui::TextDisabled("%s",state_->error.c_str()); ImGui::PopTextWrapPos(); }
}
