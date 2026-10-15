#include "pipe_server.hpp"
#include "module_adapter.hpp"
#include "protocol.hpp"
#include "../modules/core/module_registry.hpp"
#include "../modules/core/public_config.hpp"
#include "../modules/render/hud.hpp"
#include "../gui/gui.hpp"
#include "../configs/configs.hpp"
#include "../friends/friends.hpp"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>

namespace
{
    constexpr std::uint32_t bootstrap_magic = 0x54484452;
    struct bootstrap_data { std::uint32_t magic{}; wchar_t pipe_name[96]{}; wchar_t nonce[96]{}; };

    bool load_bootstrap(std::wstring& pipe_name, std::string& nonce)
    {
        const std::wstring mapping_name = L"Local\\ThedoriusSession-" + std::to_wstring(GetCurrentProcessId());
        HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mapping_name.c_str());
        if (!mapping) return false;
        const auto* data = static_cast<const bootstrap_data*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(bootstrap_data)));
        if (!data) { CloseHandle(mapping); return false; }
        const bool valid = data->magic == bootstrap_magic && data->pipe_name[0] && data->nonce[0];
        if (valid)
        {
            pipe_name = data->pipe_name;
            const std::wstring wide_nonce = data->nonce;
            nonce.clear(); nonce.reserve(wide_nonce.size());
            for (const wchar_t character : wide_nonce) nonce.push_back(static_cast<char>(character));
        }
        UnmapViewOfFile(data); CloseHandle(mapping); return valid;
    }

    bool read_exact(HANDLE pipe, void* target, DWORD length)
    {
        auto* bytes = static_cast<unsigned char*>(target); DWORD total = 0;
        while (total < length)
        {
            DWORD received = 0;
            if (!ReadFile(pipe, bytes + total, length - total, &received, nullptr) || received == 0) return false;
            total += received;
        }
        return true;
    }

    bool write_all(HANDLE pipe, const std::string& data)
    {
        DWORD total = 0;
        while (total < data.size())
        {
            DWORD written = 0;
            if (!WriteFile(pipe, data.data() + total, static_cast<DWORD>(data.size() - total), &written, nullptr) || written == 0) return false;
            total += written;
        }
        return true;
    }

    std::string config_snapshot_json()
    {
        const configs::snapshot snapshot = configs::get_snapshot();
        std::string json = "["; bool first = true;
        for (const auto& entry : snapshot.entries)
        {
            if (!first) json += ','; first = false;
            json += "{\"id\":" + std::to_string(entry.id) + ",\"name\":\"" + ui_bridge::protocol::escape_json(entry.name) +
                "\",\"author\":\"" + ui_bridge::protocol::escape_json(entry.author_name) + "\",\"description\":\"" +
                ui_bridge::protocol::escape_json(entry.description) + "\",\"modules\":" + std::to_string(entry.module_count) +
                ",\"downloads\":" + std::to_string(entry.downloads) + ",\"mine\":" + (entry.mine ? "true" : "false") +
                ",\"favourite\":false,\"updated\":\"" + ui_bridge::protocol::escape_json(entry.updated_at) + "\"}";
        }
        return json + ']';
    }

    std::pair<std::string, std::string> friends_snapshot_json()
    {
        std::string players = "["; bool first = true;
        for (const auto& entry : friends::server_players_snapshot())
        {
            if (!first) players += ','; first = false;
            players += "{\"name\":\"" + ui_bridge::protocol::escape_json(entry.name) + "\",\"local\":" +
                (entry.local ? "true" : "false") + ",\"friend\":" + (entry.friend_added ? "true" : "false") + '}';
        }
        players += ']';
        std::string saved = "["; first = true;
        for (const std::string& name : friends::friend_names_snapshot())
        {
            if (!first) saved += ','; first = false;
            saved += "{\"name\":\"" + ui_bridge::protocol::escape_json(name) + "\",\"online\":true}";
        }
        return { players, saved + ']' };
    }

    std::string hud_snapshot_json()
    {
        std::string player = "Player"; int ping = 0;
        if (auto* watermark = dynamic_cast<modules::hud*>(modules::find_module_by_id("watermark")))
        {
            auto snapshot = watermark->status_snapshot(); player = std::move(snapshot.first); ping = snapshot.second;
        }
        return "{\"playerName\":\"" + ui_bridge::protocol::escape_json(player) + "\",\"ping\":" +
            std::to_string(std::clamp(ping, 0, 9999)) + '}';
    }
}

ui_bridge::pipe_server& ui_bridge::pipe_server::instance() { static pipe_server value; return value; }
ui_bridge::pipe_server::~pipe_server() { stop(); }

bool ui_bridge::pipe_server::start()
{
    if (running_.exchange(true)) return true;
    if (!load_bootstrap(pipe_name_, nonce_)) { running_ = false; return false; }
    publish_snapshot();
    thread_ = std::thread(&pipe_server::worker, this);
    return true;
}

void ui_bridge::pipe_server::stop()
{
    if (!running_.exchange(false)) return;
    const std::wstring full_name = L"\\\\.\\pipe\\" + pipe_name_;
    if (HANDLE wake = CreateFileW(full_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr); wake != INVALID_HANDLE_VALUE) CloseHandle(wake);
    if (thread_.joinable()) thread_.join();
    connected_ = false; hud_ready_ = false;
}

void ui_bridge::pipe_server::publish_snapshot()
{
    const std::string value = ui_bridge::module_snapshot_json();
    std::scoped_lock lock(mutex_); snapshot_ = value;
}

void ui_bridge::pipe_server::pump()
{
    static auto next_snapshot = std::chrono::steady_clock::now();
    if (connected_)
    {
        gui::draw = false;
        const bool focused = gui::get_window() && GetForegroundWindow() == gui::get_window();
        const bool menu_down = (GetAsyncKeyState(gui::menu_key) & 0x8000) != 0;
        if (focused && menu_down && !menu_key_was_down_)
        {
            menu_visible_ = !menu_visible_.load();
            menu_event_pending_ = true;
        }
        menu_key_was_down_ = menu_down;
    }
    std::queue<command> pending;
    { std::scoped_lock lock(mutex_); std::swap(pending, commands_); }
    bool changed = false;
    while (!pending.empty())
    {
        const command current = std::move(pending.front()); pending.pop();
        if (current.type == command::kind::menu_visibility) { menu_visible_ = current.flag; continue; }
        if (current.type == command::kind::config_refresh) { configs::refresh(); continue; }
        if (current.type == command::kind::config_load) { configs::load(current.number); continue; }
        if (current.type == command::kind::config_delete) { configs::remove(current.number); continue; }
        if (current.type == command::kind::config_publish)
        {
            configs::publish(current.number, current.value, current.secondary, current.tertiary,
                modules::export_public_config(), modules::enabled_module_count());
            continue;
        }
        if (current.type == command::kind::friend_add) { friends::add(current.value); continue; }
        if (current.type == command::kind::friend_remove) { friends::remove(current.value); continue; }
        if (current.type == command::kind::begin_keybind || current.type == command::kind::begin_menu_key)
        {
            keybind_capture_id_ = current.type == command::kind::begin_menu_key ? "__menu_key__" : current.module_id;
            for (int key = 1; key < 256; ++key) key_was_down_[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
            continue;
        }
        if (current.type == command::kind::set_keybind)
        {
            if (modules::module* owner = modules::find_module_by_id(current.module_id)) { owner->set_keybind(current.number); changed = true; }
            continue;
        }
        modules::module* owner = modules::find_module_by_id(current.module_id);
        if (!owner) continue;
        if (current.type == command::kind::set_enabled) { owner->enabled = current.flag; changed = true; }
        else if (current.type == command::kind::set_setting) changed |= apply_setting(*owner, current.setting_id, current.value);
    }
    if (!keybind_capture_id_.empty())
    {
        for (int key = 1; key < 256; ++key)
        {
            const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
            if (down && !key_was_down_[key])
            {
                if (keybind_capture_id_ == "__menu_key__")
                {
                    gui::menu_key = key == VK_ESCAPE ? VK_INSERT : key; changed = true;
                }
                else if (modules::module* owner = modules::find_module_by_id(keybind_capture_id_))
                {
                    owner->set_keybind(key == VK_ESCAPE ? 0 : key); changed = true;
                }
                keybind_capture_id_.clear(); break;
            }
            key_was_down_[key] = down;
        }
    }
    std::string downloaded_payload, downloaded_name, apply_message;
    if (configs::consume_pending_payload(downloaded_payload, downloaded_name))
    {
        const bool applied = modules::apply_public_config(downloaded_payload, apply_message);
        changed |= applied;
    }
    const auto now = std::chrono::steady_clock::now();
    if (changed || now >= next_snapshot)
    {
        publish_snapshot();
        next_snapshot = now + std::chrono::milliseconds(500);
    }
}

void ui_bridge::pipe_server::queue_message(const std::string& json)
{
    command next;
    const std::string type = protocol::string_value(json, "messageType");
    if (type == "set_module_enabled")
    {
        next.type = command::kind::set_enabled; next.module_id = protocol::string_value(json, "id"); next.flag = protocol::bool_value(json, "enabled");
    }
    else if (type == "set_setting")
    {
        next.type = command::kind::set_setting; next.module_id = protocol::string_value(json, "moduleId"); next.setting_id = protocol::string_value(json, "settingId"); next.value = protocol::raw_value(json, "value");
    }
    else if (type == "begin_keybind") { next.type = command::kind::begin_keybind; next.module_id = protocol::string_value(json, "id"); }
    else if (type == "begin_menu_key") { next.type = command::kind::begin_menu_key; }
    else if (type == "set_keybind") { next.type = command::kind::set_keybind; next.module_id = protocol::string_value(json, "id"); next.number = std::atoi(protocol::raw_value(json, "keybind").c_str()); }
    else if (type == "menu_visibility") { next.type = command::kind::menu_visibility; next.flag = protocol::bool_value(json, "visible"); }
    else if (type == "config_refresh") next.type = command::kind::config_refresh;
    else if (type == "config_load") { next.type = command::kind::config_load; next.number = std::atoi(protocol::raw_value(json, "id").c_str()); }
    else if (type == "config_delete") { next.type = command::kind::config_delete; next.number = std::atoi(protocol::raw_value(json, "id").c_str()); }
    else if (type == "config_publish")
    {
        next.type = command::kind::config_publish; next.number = std::atoi(protocol::raw_value(json, "id").c_str());
        next.value = protocol::string_value(json, "name"); next.secondary = protocol::string_value(json, "description"); next.tertiary = protocol::string_value(json, "author");
    }
    else if (type == "friends_action")
    {
        next.type = protocol::string_value(json, "action") == "remove" ? command::kind::friend_remove : command::kind::friend_add;
        next.value = protocol::string_value(json, "name");
    }
    else return;
    std::scoped_lock lock(mutex_); commands_.push(std::move(next));
}

void ui_bridge::pipe_server::worker()
{
    const std::wstring full_name = L"\\\\.\\pipe\\" + pipe_name_;
    while (running_)
    {
        HANDLE pipe = CreateNamedPipeW(full_name.c_str(), PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1, 1024 * 1024, 1024 * 1024, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) break;
        const BOOL connected = ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
        if (!connected) { CloseHandle(pipe); continue; }
        connected_ = true;
        auto next_push = std::chrono::steady_clock::now();
        while (running_)
        {
            DWORD available = 0;
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) break;
            if (std::chrono::steady_clock::now() >= next_push)
            {
                next_push = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                std::string snapshot; { std::scoped_lock lock(mutex_); snapshot = snapshot_; }
                const std::string modules = "{\"protocol\":1,\"messageType\":\"module_snapshot\",\"requestId\":\"pulse\",\"sessionNonce\":" +
                    ('"' + protocol::escape_json(nonce_) + '"') + ",\"payload\":{\"modules\":" + snapshot + "}}";
                const std::string hud = "{\"protocol\":1,\"messageType\":\"hud_snapshot\",\"requestId\":\"pulse-hud\",\"sessionNonce\":" +
                    ('"' + protocol::escape_json(nonce_) + '"') + ",\"payload\":" + hud_snapshot_json() + '}';
                const std::string configs_message = "{\"protocol\":1,\"messageType\":\"configs_snapshot\",\"requestId\":\"pulse-configs\",\"sessionNonce\":" +
                    ('"' + protocol::escape_json(nonce_) + '"') + ",\"payload\":{\"configs\":" + config_snapshot_json() + "}}";
                const auto [players, saved] = friends_snapshot_json();
                const std::string friends_message = "{\"protocol\":1,\"messageType\":\"friends_snapshot\",\"requestId\":\"pulse-friends\",\"sessionNonce\":" +
                    ('"' + protocol::escape_json(nonce_) + '"') + ",\"payload\":{\"serverPlayers\":" + players + ",\"friends\":" + saved + "}}";
                if (!write_all(pipe, protocol::frame(modules)) || !write_all(pipe, protocol::frame(hud)) ||
                    !write_all(pipe, protocol::frame(configs_message)) || !write_all(pipe, protocol::frame(friends_message))) break;
            }
            if (menu_event_pending_.exchange(false))
            {
                const std::string event = "{\"protocol\":1,\"messageType\":\"menu_visibility\",\"requestId\":\"menu-key\",\"sessionNonce\":" +
                    ('"' + protocol::escape_json(nonce_) + '"') + ",\"payload\":{\"visible\":" + (menu_visible_ ? "true" : "false") + "}}";
                if (!write_all(pipe, protocol::frame(event))) break;
            }
            if (available < sizeof(std::uint32_t)) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }
            std::uint32_t length = 0;
            if (!read_exact(pipe, &length, sizeof(length)) || length == 0 || length > 1024 * 1024) break;
            std::string json(length, '\0'); if (!read_exact(pipe, json.data(), length)) break;
            if (protocol::string_value(json, "sessionNonce") != nonce_) break;
            const std::string type = protocol::string_value(json, "messageType");
            const std::string request = protocol::string_value(json, "requestId");
            if (type == "hello")
            {
                hud_ready_ = true;
                std::string snapshot; { std::scoped_lock lock(mutex_); snapshot = snapshot_; }
                const std::string ack = "{\"protocol\":1,\"messageType\":\"hello_ack\",\"requestId\":" +
                    ('"' + protocol::escape_json(request) + '"') + ",\"sessionNonce\":" + ('"' + protocol::escape_json(nonce_) + '"') + ",\"payload\":{\"pid\":" + std::to_string(GetCurrentProcessId()) + "}}";
                if (!write_all(pipe, protocol::frame(ack))) break;
                const std::string modules = "{\"protocol\":1,\"messageType\":\"module_snapshot\",\"requestId\":\"initial\",\"sessionNonce\":" +
                    ('"' + protocol::escape_json(nonce_) + '"') + ",\"payload\":{\"modules\":" + snapshot + "}}";
                if (!write_all(pipe, protocol::frame(modules))) break;
                const std::string configs_message = "{\"protocol\":1,\"messageType\":\"configs_snapshot\",\"requestId\":\"initial-configs\",\"sessionNonce\":" +
                    ('"' + protocol::escape_json(nonce_) + '"') + ",\"payload\":{\"configs\":" + config_snapshot_json() + "}}";
                if (!write_all(pipe, protocol::frame(configs_message))) break;
                const auto [players, saved] = friends_snapshot_json();
                const std::string friends_message = "{\"protocol\":1,\"messageType\":\"friends_snapshot\",\"requestId\":\"initial-friends\",\"sessionNonce\":" +
                    ('"' + protocol::escape_json(nonce_) + '"') + ",\"payload\":{\"serverPlayers\":" + players + ",\"friends\":" + saved + "}}";
                if (!write_all(pipe, protocol::frame(friends_message))) break;
            }
            else queue_message(json);
        }
        connected_ = false; hud_ready_ = false; FlushFileBuffers(pipe); DisconnectNamedPipe(pipe); CloseHandle(pipe);
    }
}
