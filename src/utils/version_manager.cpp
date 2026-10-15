#include "version_manager.hpp"
#include <Windows.h>
#include <algorithm>
#include <iostream>
#include <string>

namespace mc
{
    static version g_current_version = version::v1_21_1;
    static std::string g_version_str = "1.21.1";
    static std::string g_runtime_label = "NATIVE RUNTIME  //  1.21.1";

    version get_version()
    {
        return g_current_version;
    }

    const char* get_version_name()
    {
        return g_version_str.c_str();
    }

    const char* get_runtime_label()
    {
        return g_runtime_label.c_str();
    }

    bool is_1_21_1()
    {
        return g_current_version == version::v1_21_1;
    }

    bool is_1_21_11()
    {
        return g_current_version == version::v1_21_11;
    }

    void set_version(version ver)
    {
        g_current_version = ver;
        if (ver == version::v1_21_11)
        {
            g_version_str = "1.21.11";
            g_runtime_label = "NATIVE RUNTIME  //  1.21.11";
        }
        else
        {
            g_current_version = version::v1_21_1;
            g_version_str = "1.21.1";
            g_runtime_label = "NATIVE RUNTIME  //  1.21.1";
        }
    }

    static std::string probe_window_title()
    {
        std::string found_title;
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            if (!IsWindowVisible(hwnd)) return TRUE;
            char title[512]{};
            GetWindowTextA(hwnd, title, sizeof(title));
            std::string st = title;
            if (st.find("Minecraft") != std::string::npos || st.find("Fabric") != std::string::npos)
            {
                auto* out = reinterpret_cast<std::string*>(lParam);
                *out = st;
                return FALSE; // stop enum
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&found_title));

        return found_title;
    }

    static std::string jstring_to_utf8(JNIEnv* env, jstring jstr)
    {
        if (!env || !jstr) return {};
        const char* chars = env->GetStringUTFChars(jstr, nullptr);
        if (!chars) return {};
        std::string res(chars);
        env->ReleaseStringUTFChars(jstr, chars);
        return res;
    }

    version detect(JNIEnv* env, jvmti* jvmti_env)
    {
        (void)jvmti_env;
        std::string detected_str;

        // Method 1: Check MinecraftClient.getGameVersion() or SharedConstants
        if (env)
        {
            // Probe net.minecraft.class_155 (SharedConstants)
            jclass shared_constants = env->FindClass("net/minecraft/class_155");
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
            }

            if (shared_constants)
            {
                // method_16673 ()Lnet/minecraft/class_6489;
                jmethodID get_game_version = env->GetStaticMethodID(shared_constants, "method_16673", "()Lnet/minecraft/class_6489;");
                if (env->ExceptionCheck()) env->ExceptionClear();

                if (get_game_version)
                {
                    jobject gv_obj = env->CallStaticObjectMethod(shared_constants, get_game_version);
                    if (env->ExceptionCheck()) env->ExceptionClear();

                    if (gv_obj)
                    {
                        jclass gv_cls = env->GetObjectClass(gv_obj);
                        // method_37987 or comp_617 ()Ljava/lang/String;
                        jmethodID get_name = env->GetMethodID(gv_cls, "method_37987", "()Ljava/lang/String;");
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        if (!get_name)
                        {
                            get_name = env->GetMethodID(gv_cls, "comp_617", "()Ljava/lang/String;");
                            if (env->ExceptionCheck()) env->ExceptionClear();
                        }
                        if (!get_name)
                        {
                            get_name = env->GetMethodID(gv_cls, "getName", "()Ljava/lang/String;");
                            if (env->ExceptionCheck()) env->ExceptionClear();
                        }

                        if (get_name)
                        {
                            jstring name_str = (jstring)env->CallObjectMethod(gv_obj, get_name);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            if (name_str)
                            {
                                detected_str = jstring_to_utf8(env, name_str);
                                env->DeleteLocalRef(name_str);
                            }
                        }
                        env->DeleteLocalRef(gv_cls);
                        env->DeleteLocalRef(gv_obj);
                    }
                }
                env->DeleteLocalRef(shared_constants);
            }

            // Fallback Method 2: Check MinecraftClient.getGameVersion()
            if (detected_str.empty())
            {
                jclass mc_cls = env->FindClass("net/minecraft/class_310");
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (mc_cls)
                {
                    jfieldID inst_fid = env->GetStaticFieldID(mc_cls, "field_1700", "Lnet/minecraft/class_310;");
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    if (inst_fid)
                    {
                        jobject mc_inst = env->GetStaticObjectField(mc_cls, inst_fid);
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        if (mc_inst)
                        {
                            jmethodID get_ver_mid = env->GetMethodID(mc_cls, "method_1547", "()Ljava/lang/String;");
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            if (get_ver_mid)
                            {
                                jstring v_str = (jstring)env->CallObjectMethod(mc_inst, get_ver_mid);
                                if (env->ExceptionCheck()) env->ExceptionClear();
                                if (v_str)
                                {
                                    detected_str = jstring_to_utf8(env, v_str);
                                    env->DeleteLocalRef(v_str);
                                }
                            }
                            env->DeleteLocalRef(mc_inst);
                        }
                    }
                    env->DeleteLocalRef(mc_cls);
                }
            }
        }

        // Method 3: Fallback to Window Title
        if (detected_str.empty())
        {
            detected_str = probe_window_title();
        }

        // Evaluate version string
        if (!detected_str.empty())
        {
            if (detected_str.find("1.21.11") != std::string::npos)
            {
                set_version(version::v1_21_11);
                return version::v1_21_11;
            }
            if (detected_str.find("1.21.1") != std::string::npos)
            {
                set_version(version::v1_21_1);
                return version::v1_21_1;
            }
        }

        // Default: 1.21.1 (keeps 100% backward compatibility)
        set_version(version::v1_21_1);
        return version::v1_21_1;
    }
}
