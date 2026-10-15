#pragma once
#include <jni.h>
#include <string>

namespace java_hook
{
	bool init();
	void shutdown();
    using callback = bool(*)(JNIEnv*, int, jobject, jobject, jobject);
    bool ensure_vape(JNIEnv* env, jobject client, callback handler);
    bool vape_ready();
    std::string vape_error();
}
