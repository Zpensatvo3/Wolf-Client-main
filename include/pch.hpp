#pragma once

// The PCH must contain only stable platform/vendor headers. Project headers
// used to live here, which invalidated every object file whenever one module
// declaration changed and turned small edits into full 50-second rebuilds.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <jni.h>
#include <jvmti.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
