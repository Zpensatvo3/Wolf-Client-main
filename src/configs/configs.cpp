#include "configs.hpp"
#include "../modules/core/config_json.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
    namespace json = modules::config_json;
    std::mutex state_mutex;
    std::filesystem::path directory;
    configs::snapshot state;
    std::string pending_payload, pending_name;
    bool running = false;
    constexpr std::size_t maximum_bytes = 96 * 1024;
    std::string json_string(const std::string& value) { return "\"" + json::escape(value) + "\""; }
    std::filesystem::path file(int id)
    {
        if (id <= 0) throw std::runtime_error("Invalid profile ID.");
        return directory / (std::to_string(id) + ".json");
    }
    std::string read(const std::filesystem::path& path)
    {
        if (!std::filesystem::is_regular_file(path) || std::filesystem::is_symlink(path) ||
            std::filesystem::file_size(path) > maximum_bytes) throw std::runtime_error("Invalid profile file.");
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw std::runtime_error("Cannot read profile.");
        std::string text((std::istreambuf_iterator<char>(stream)), {});
        if (stream.bad()) throw std::runtime_error("Cannot read profile.");
        return text;
    }
    std::string field(const json::value& value, const char* key)
    {
        const auto* found = value.find(key);
        if (!found || found->type != json::kind::string) throw std::runtime_error("Invalid profile metadata.");
        return found->text;
    }
    void write_profile(const std::filesystem::path& destination, const std::string& text)
    {
        auto temporary = destination; temporary += ".tmp";
        if (std::filesystem::is_symlink(temporary)) throw std::runtime_error("Invalid temporary profile path.");
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            stream.write(text.data(), static_cast<std::streamsize>(text.size())); stream.close();
            if (!stream) throw std::runtime_error("Could not save local profile.");
        }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace local profile.");
#else
        std::filesystem::rename(temporary, destination);
#endif
    }
    void refresh_entries()
    {
        std::vector<configs::public_config> entries;
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.path().extension() != ".json") continue;
            const auto text = read(entry.path());
            const auto document = json::parse(text);
            if (!document) throw std::runtime_error("A local profile contains invalid JSON.");
            const auto original_payload = field(*document, "payload");
            const auto clean_payload = json::without_keybinds(original_payload);
            if (!clean_payload) throw std::runtime_error("Invalid profile payload.");
            if (*clean_payload != original_payload) {
                std::string clean = "{";
                for (const auto& [key, value] : document->members) {
                    if (clean.size() > 1) clean += ',';
                    clean += json_string(key) + ':' + (key == "payload" ? json_string(*clean_payload) : std::string(value.raw));
                }
                write_profile(entry.path(), clean + '}');
            }
            const auto* id = document->find("id");
            const auto number = id ? id->integer() : std::nullopt;
            if (!number || *number <= 0 || file(*number).filename() != entry.path().filename())
                throw std::runtime_error("Invalid profile ID.");
            configs::public_config info;
            info.id = *number;
            info.name = field(*document, "name");
            info.description = field(*document, "description");
            info.author_name = field(*document, "author_name");
            info.created_at = field(*document, "created_at");
            info.updated_at = field(*document, "updated_at");
            const auto* count = document->find("module_count");
            info.module_count = count ? count->integer().value_or(0) : 0;
            info.mine = true;
            entries.push_back(std::move(info));
        }
        state.entries = std::move(entries);
    }
    template<class Work> void operation(Work work)
    {
        std::lock_guard lock(state_mutex);
        if (!running) return;
        try { work(); state.success = true; }
        catch (const std::exception& error) { state.status = error.what(); state.success = false; }
        state.busy = false;
        ++state.revision;
    }
}

void configs::initialize(const std::filesystem::path& location)
{
    std::lock_guard lock(state_mutex);
    state = {};
    pending_payload.clear(); pending_name.clear();
    directory = location;
    running = true;
    try
    {
        if (directory.empty())
        {
#ifdef _WIN32
            wchar_t appdata[32768]{};
            const auto size = GetEnvironmentVariableW(L"APPDATA", appdata, 32768);
            if (!size || size >= 32768) throw std::runtime_error("APPDATA is unavailable; local profiles cannot be opened.");
            directory = std::filesystem::path(appdata) / L"OneStep" / L"configs";
#else
            const char* appdata = std::getenv("APPDATA");
            if (!appdata || !*appdata) throw std::runtime_error("APPDATA is unavailable; local profiles cannot be opened.");
            directory = std::filesystem::path(appdata) / "OneStep" / "configs";
#endif
        }
        std::filesystem::create_directories(directory);
        refresh_entries();
        state.status = "Local profiles ready.";
    }
    catch (const std::exception& error) { running = false; state.success = false; state.status = error.what(); }
    ++state.revision;
}
void configs::shutdown()
{
    std::lock_guard lock(state_mutex);
    running = false;
    pending_payload.clear(); pending_name.clear();
}
configs::snapshot configs::get_snapshot() { std::lock_guard lock(state_mutex); return state; }
void configs::refresh() { operation([] { refresh_entries(); state.status = "Local profiles refreshed."; }); }
void configs::publish(int id, const std::string& name, const std::string& description,
    const std::string& author_name, const std::string& payload, int module_count)
{
    operation([&] {
        const auto parsed = json::parse(payload);
        const auto clean_payload = json::without_keybinds(payload);
        const auto* list = parsed ? parsed->find("modules") : nullptr;
        if (!clean_payload || !list || list->type != json::kind::array || name.empty() || name.size() > 128 ||
            description.size() > 4096 || author_name.size() > 128 || module_count < 0 || module_count > 128)
            throw std::runtime_error("Invalid local profile.");
        refresh_entries();
        std::string created;
        if (id > 0)
        {
            const auto previous = read(file(id));
            const auto document = json::parse(previous);
            if (!document) throw std::runtime_error("Invalid existing profile.");
            created = field(*document, "created_at");
        }
        else
        {
            if (id < 0) throw std::runtime_error("Invalid profile ID.");
            id = 1;
            for (const auto& entry : state.entries)
            {
                if (entry.id == std::numeric_limits<int>::max()) throw std::runtime_error("Profile ID limit reached.");
                id = (std::max)(id, entry.id + 1);
            }
        }
        const std::chrono::year_month_day date(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
        const auto two = [](unsigned n) { return (n < 10 ? "0" : "") + std::to_string(n); };
        const auto stamp = std::to_string(int(date.year())) + "-" + two(unsigned(date.month())) + "-" + two(unsigned(date.day()));
        if (created.empty()) created = stamp;
        const auto text = "{\"id\":" + std::to_string(id) + ",\"name\":" + json_string(name) +
            ",\"description\":" + json_string(description) + ",\"author_name\":" + json_string(author_name) +
            ",\"created_at\":" + json_string(created) + ",\"updated_at\":" + json_string(stamp) +
            ",\"module_count\":" + std::to_string(module_count) + ",\"payload\":" + json_string(*clean_payload) + "}";
        if (text.size() > maximum_bytes) throw std::runtime_error("Profile exceeds the 96 KB limit.");
        write_profile(file(id), text);
        refresh_entries();
        state.status = "Local profile saved.";
    });
}
void configs::load(int id)
{
    operation([&] {
        pending_payload.clear(); pending_name.clear();
        const auto text = read(file(id));
        const auto document = json::parse(text);
        if (!document) throw std::runtime_error("Invalid profile JSON.");
        const auto payload = field(*document, "payload");
        const auto parsed = json::parse(payload);
        const auto* list = parsed ? parsed->find("modules") : nullptr;
        if (!list || list->type != json::kind::array) throw std::runtime_error("Invalid profile payload.");
        pending_name = field(*document, "name");
        pending_payload = payload;
        state.status = "Local profile loaded.";
    });
}
void configs::remove(int id)
{
    operation([&] {
        if (!std::filesystem::remove(file(id))) throw std::runtime_error("Profile not found.");
        refresh_entries(); state.status = "Local profile deleted.";
    });
}
bool configs::consume_pending_payload(std::string& payload, std::string& name)
{
    std::lock_guard lock(state_mutex);
    if (pending_payload.empty()) return false;
    payload = std::move(pending_payload); name = std::move(pending_name);
    pending_payload.clear(); pending_name.clear();
    return true;
}
