#pragma once
#include <jni.h>
#include <string>

class jvmti;

namespace mc
{
    enum class version
    {
        unknown,
        v1_21_1,
        v1_21_11
    };

    version get_version();
    const char* get_version_name();
    const char* get_runtime_label();
    bool is_1_21_1();
    bool is_1_21_11();

    // Detects Minecraft version using JNI, JVMTI, or Window title probing.
    version detect(JNIEnv* env, jvmti* jvmti_env);

    // Explicit override if needed
    void set_version(version ver);
}
