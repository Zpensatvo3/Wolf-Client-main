#pragma once
#include "notification_model.hpp"
#include <string>

namespace services::notifications
{
    void configure(bool enabled, module_settings::notifications settings);
    void clear();
    void module_changed(const char* id, const char* name, bool enabled);
    void operation_result(const std::string& message, bool success = true);
    snapshot collect();
}
