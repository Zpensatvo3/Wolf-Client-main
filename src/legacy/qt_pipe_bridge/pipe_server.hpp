#pragma once

#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace ui_bridge
{
    struct command
    {
        enum class kind { set_enabled, set_setting, set_keybind, begin_keybind, begin_menu_key, menu_visibility, config_refresh, config_load, config_delete, config_publish, friend_add, friend_remove } type{};
        std::string module_id;
        std::string setting_id;
        std::string value;
        std::string secondary;
        std::string tertiary;
        int number{};
        bool flag{};
    };

    class pipe_server final
    {
    public:
        static pipe_server& instance();
        bool start();
        void stop();
        void pump();
        void publish_snapshot();
        bool connected() const { return connected_.load(); }
        bool qml_hud_ready() const { return hud_ready_.load(); }
        bool menu_visible() const { return menu_visible_.load(); }

    private:
        void worker();
        void queue_message(const std::string& json);
        pipe_server() = default;
        ~pipe_server();
        std::atomic_bool running_{ false };
        std::atomic_bool connected_{ false };
        std::atomic_bool hud_ready_{ false };
        std::atomic_bool menu_visible_{ false };
        std::atomic_bool menu_event_pending_{ false };
        std::thread thread_;
        std::mutex mutex_;
        std::queue<command> commands_;
        std::string snapshot_;
        std::wstring pipe_name_;
        std::string nonce_;
        std::string keybind_capture_id_;
        bool key_was_down_[256]{};
        bool menu_key_was_down_ = false;
    };
}
