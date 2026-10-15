#include "notification_service.hpp"
#include <chrono>
#include <functional>
#include <mutex>
#include <string_view>

namespace
{
    std::mutex mutex;
    bool active = false;
    module_settings::notifications config;
    services::notifications::queue messages;
    double now_seconds()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
}
void services::notifications::configure(bool enabled, module_settings::notifications settings)
{
    std::lock_guard lock(mutex);
    if (!enabled) { active = false; messages.clear(); return; }
    if (!modules::settings::normalize(settings, module_settings::notifications::schema())) return;
    active = true; config = settings;
}
void services::notifications::clear() { std::lock_guard lock(mutex); messages.clear(); }
void services::notifications::module_changed(const char* id, const char* name, bool enabled)
{
    if (!id || !name || !*id || !*name || std::string_view(id) == "panic" ||
        std::string_view(id) == "click_gui" || std::string_view(id) == "notifications") return;
    std::lock_guard lock(mutex);
    if (!active || !config.module_changes) return;
    std::string title = name;
    bool upper = true;
    for (char& c : title) { if (upper && c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A'); upper = c == ' '; }
    messages.push({0, "module:" + std::string(id), std::move(title), {}, enabled ? kind::module_on : kind::module_off}, now_seconds(), config.duration);
}
void services::notifications::operation_result(const std::string& message, bool success)
{
    if (message.empty()) return;
    std::lock_guard lock(mutex);
    if (!active || !config.operation_results) return;
    const auto key = "operation:" + std::to_string(std::hash<std::string>{}(message)) + (success ? ":ok" : ":error");
    messages.push({0, key, {}, message, success ? kind::success : kind::error}, now_seconds(), config.duration);
}
services::notifications::snapshot services::notifications::collect()
{
    std::lock_guard lock(mutex);
    const double now = now_seconds();
    return {config, active ? messages.collect(now, config) : std::vector<entry>{}, now};
}
