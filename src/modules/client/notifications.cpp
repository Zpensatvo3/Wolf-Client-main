#include "notifications.hpp"
#include "../../services/notifications/notification_service.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../gui/gui.hpp"
#include "../ui_redirect.hpp"

modules::notifications::notifications() : module("notifications", "Client", "notifications")
{
    enabled = true;
    services::notifications::configure(true, settings_.read());
}
modules::notifications::~notifications() { services::notifications::configure(false, settings_.read()); }
void modules::notifications::run(::cache&) { services::notifications::configure(enabled, settings_.read()); }
void modules::notifications::on_disable(::cache&) { services::notifications::configure(false, settings_.read()); }
void modules::notifications::render_options()
{
    gui::settings::render(settings_, module_settings::notifications::schema());
    ImGui::TextWrapped("%s", gui::text("Uses the active theme and Reduce Motion preference. Always silent.",
        "Aktif temayi ve Azaltilmis Hareket tercihini kullanir. Her zaman sessizdir."));
}
