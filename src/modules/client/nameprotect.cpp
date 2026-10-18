#include "nameprotect.hpp"
#include "../../gui/gui.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/client_context.hpp"
#include "../../../resource.h"
#include <atomic>
#include <cstring>
#include <span>
#include <stdexcept>

extern JavaVM* jvm;

namespace
{
    std::span<const unsigned char> embedded(int id)
    {
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&embedded),&module);
        const auto entry=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
        if (!entry) return {};
        return {static_cast<const unsigned char*>(LockResource(LoadResource(module,entry))),SizeofResource(module,entry)};
    }
    template<class Value> Value checked(JNIEnv* env,Value value)
    {
        if (env->ExceptionCheck()) { env->ExceptionClear(); throw std::runtime_error("JNI"); }
        if (!value) throw std::runtime_error("JNI");
        return value;
    }
}

struct modules::nameprotect::runtime
{
    inline static std::atomic<runtime*> transforming{nullptr};
    jvmtiEnv* tooling=nullptr;
    jclass font=nullptr,bridge=nullptr,transformer=nullptr;
    jmethodID configure=nullptr,transform=nullptr;
    std::string name,alias,error;
    bool installed=false,failed=false,changed=false;

    static void JNICALL class_bytes(jvmtiEnv* tools,JNIEnv* env,jclass redefining,jobject,
        const char* class_name,jobject,jint size,const unsigned char* bytes,jint* new_size,unsigned char** new_bytes)
    {
        auto* self=transforming.load(std::memory_order_acquire);
        if (!self || !redefining || !class_name || std::strcmp(class_name,ONESTEP_MC_NAME("net/minecraft/class_327"))!=0 ||
            !env->IsSameObject(redefining,self->font)) return;
        // Retransformation is synchronous. No native hook remains on the render hot path.
        if (env->PushLocalFrame(8)!=JNI_OK) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
        auto original=env->NewByteArray(size);
        if (original) env->SetByteArrayRegion(original,0,size,reinterpret_cast<const jbyte*>(bytes));
        auto result=original && !env->ExceptionCheck()
            ? static_cast<jbyteArray>(env->CallStaticObjectMethod(self->transformer,self->transform,original,static_cast<jint>(mc::target.build_id))) : nullptr;
        if (result && !env->ExceptionCheck())
        {
            const jint length=env->GetArrayLength(result);
            unsigned char* buffer=nullptr;
            if (length>0 && length<size*4+65536 && tools->Allocate(length,&buffer)==JVMTI_ERROR_NONE)
            {
                env->GetByteArrayRegion(result,0,length,reinterpret_cast<jbyte*>(buffer));
                if (!env->ExceptionCheck()) { *new_size=length; *new_bytes=buffer; self->changed=true; }
                else tools->Deallocate(buffer);
            }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->PopLocalFrame(nullptr);
    }

    void disable(JNIEnv* env)
    {
        services::privacy::publish();
        if (bridge && configure)
        {
            auto empty=env->NewStringUTF("");
            if (empty) { env->CallStaticVoidMethod(bridge,configure,empty,empty); env->DeleteLocalRef(empty); }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        name.clear(); alias.clear();
    }
    void shutdown(JNIEnv* env)
    {
        disable(env);
        if (tooling)
        {
            tooling->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            // With our callback disabled, JVMTI rebuilds the class without this
            // environment's edits, preserving transformations from other agents.
            if (installed && font) tooling->RetransformClasses(1,&font);
            tooling->DisposeEnvironment(); tooling=nullptr;
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        for (auto* ref:{&font,&bridge,&transformer}) if (*ref) { env->DeleteGlobalRef(*ref); *ref=nullptr; }
        installed=false; configure=transform=nullptr;
    }
    bool connect(JNIEnv* env)
    {
        if (installed) return true;
        if (failed || !jvm) return false;
        try
        {
            auto game=checked(env,jni::get_cached_jclass<maps::MinecraftClient>());
            auto klass=checked(env,env->FindClass("java/lang/Class"));
            auto get_loader=checked(env,env->GetMethodID(klass,"getClassLoader","()Ljava/lang/ClassLoader;"));
            auto loader=checked(env,env->CallObjectMethod(game,get_loader));
            auto for_name=checked(env,env->GetStaticMethodID(klass,"forName","(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;"));
            const auto load=[&](const char* dotted,const char* binary,int id) {
                auto name=checked(env,env->NewStringUTF(dotted));
                auto type=static_cast<jclass>(env->CallStaticObjectMethod(klass,for_name,name,JNI_TRUE,loader));
                if (env->ExceptionCheck()) { env->ExceptionClear(); type=nullptr; }
                if (!type && id)
                {
                    auto bytes=embedded(id);
                    if (bytes.empty()) throw std::runtime_error("Resource");
                    type=env->DefineClass(binary,loader,reinterpret_cast<const jbyte*>(bytes.data()),static_cast<jsize>(bytes.size()));
                }
                return static_cast<jclass>(checked(env,env->NewGlobalRef(checked(env,type))));
            };
            bridge=load("net.onestep.privacy.NameProtectBridge","net/onestep/privacy/NameProtectBridge",IDR_NAMEPROTECT_BRIDGE);
            transformer=load("net.onestep.privacy.NameProtectTransformer","net/onestep/privacy/NameProtectTransformer",IDR_NAMEPROTECT_TRANSFORMER);
            font=load(ONESTEP_MC_NAME("net.minecraft.class_327"),nullptr,0);
            configure=checked(env,env->GetStaticMethodID(bridge,"configure","(Ljava/lang/String;Ljava/lang/String;)V"));
            auto prepare=checked(env,env->GetStaticMethodID(bridge,"prepare","(Ljava/lang/ClassLoader;I)V"));
            env->CallStaticVoidMethod(bridge,prepare,loader,static_cast<jint>(mc::target.build_id));
            if (env->ExceptionCheck()) throw std::runtime_error("Text interfaces");
            transform=checked(env,env->GetStaticMethodID(transformer,"transform","([BI)[B"));
            if (jvm->GetEnv(reinterpret_cast<void**>(&tooling),JVMTI_VERSION_1_2)!=JNI_OK || !tooling)
                throw std::runtime_error("JVMTI");
            jvmtiCapabilities capabilities{}; capabilities.can_retransform_classes=1;
            if (tooling->AddCapabilities(&capabilities)!=JVMTI_ERROR_NONE) throw std::runtime_error("Capabilities");
            jboolean modifiable=JNI_FALSE;
            if (tooling->IsModifiableClass(font,&modifiable)!=JVMTI_ERROR_NONE || !modifiable) throw std::runtime_error("Font class");
            jvmtiEventCallbacks callbacks{}; callbacks.ClassFileLoadHook=&class_bytes;
            if (tooling->SetEventCallbacks(&callbacks,sizeof(callbacks))!=JVMTI_ERROR_NONE) throw std::runtime_error("Callback");
            transforming.store(this,std::memory_order_release);
            auto status=tooling->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            if (status==JVMTI_ERROR_NONE) status=tooling->RetransformClasses(1,&font);
            tooling->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            transforming.store(nullptr,std::memory_order_release);
            if (status!=JVMTI_ERROR_NONE || !changed) throw std::runtime_error("Font hooks");
            installed=true; error.clear(); return true;
        }
        catch (...)
        {
            if (env->ExceptionCheck()) env->ExceptionClear();
            transforming.store(nullptr,std::memory_order_release);
            shutdown(env); failed=true;
            error="NameProtect could not connect to the game font.";
            return false;
        }
    }
};

modules::nameprotect::nameprotect() : module("nameprotect","Client","nameprotect"),state_(std::make_unique<runtime>()) {}
modules::nameprotect::~nameprotect() { services::privacy::publish(); }

void modules::nameprotect::on_render_shutdown()
{
    if (platform::jni_context::bind_current_thread()) state_->shutdown(jni::get_env());
    services::privacy::publish();
}
void modules::nameprotect::render_overlay()
{
    if (!platform::jni_context::bind_current_thread()) return;
    auto* env=jni::get_env();
    jni::frame frame{48};
    auto& s=*state_;
    try
    {
        if (!enabled || gui::loader_visible || gui::should_unload) { if (!s.name.empty()) s.disable(env); return; }
        auto client=maps::MinecraftClient{}.instance.get();
        if (platform::jni_context::clear_exception() || !client) { s.disable(env); return; }
        auto session=client.session.get();
        if (platform::jni_context::clear_exception() || !session) { s.disable(env); return; }
        auto username=session.getUsername.call();
        if (platform::jni_context::clear_exception() || !username) { s.disable(env); return; }
        auto name=username.to_string();
        if (!services::privacy::valid_alias(name)) { s.disable(env); return; }
        const auto config=settings_.read();
        if (name==s.name && config.alias==s.alias)
        {
            services::privacy::publish(s.name,s.alias);
            return;
        }
        if (!s.connect(env)) return;
        auto java_name=checked(env,env->NewStringUTF(name.c_str()));
        auto java_alias=checked(env,env->NewStringUTF(config.alias.c_str()));
        env->CallStaticVoidMethod(s.bridge,s.configure,java_name,java_alias);
        if (env->ExceptionCheck()) throw std::runtime_error("Configure");
        s.name=std::move(name); s.alias=config.alias;
        services::privacy::publish(s.name,s.alias);
    }
    catch (...) { if (env->ExceptionCheck()) env->ExceptionClear(); s.disable(env); }
}
void modules::nameprotect::render_options()
{
    gui::settings::edit(settings_,module_settings::nameprotect::schema(),[](auto& current) {
        char alias[17]{};
        std::memcpy(alias,current.alias.data(),current.alias.size());
        ImGui::TextUnformatted("Alias");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputText("##nameprotect_alias",alias,sizeof(alias),ImGuiInputTextFlags_CharsNoBlank) &&
            services::privacy::valid_alias(alias))
            current.alias=alias;
    });
    if (!state_->error.empty()) ImGui::TextWrapped("%s",state_->error.c_str());
}
