#include "keybind_profile.hpp"
#include "../modules/core/module_registry.hpp"
#include "../modules/core/public_config.hpp"
#include <chrono>
#include <fstream>
#include <iterator>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {
    std::filesystem::path destination;
    std::uint64_t observed = 0;
    bool dirty = false, success = true;
    configs::keybind_profile::consent choice = configs::keybind_profile::consent::pending;
    std::string message;
    std::chrono::steady_clock::time_point changed_at{};
    std::filesystem::path default_path() {
#ifdef _WIN32
        wchar_t directory[32768]{};
        const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, 32768);
        if (length && length < 32768) return std::filesystem::path(directory) / L"onestep" / L"keybinds.json";
#endif
        return {};
    }
    bool fail(const char* text) { message = text; success = false; return false; }
    std::filesystem::path consent_path() { auto path = destination; path += ".consent"; return path; }
    bool load_profile();
}

bool configs::keybind_profile::initialize(std::filesystem::path path)
{
    destination = path.empty() ? default_path() : std::move(path);
    dirty = false; success = true; message = "Keybind profile ready.";
    choice = consent::pending;
    observed = modules::keybind_revision.load(std::memory_order_acquire);
    if (destination.empty()) return fail("Local app data is unavailable; keybinds cannot be saved.");
    std::error_code error;
    const auto parent = destination.parent_path();
    if (!parent.empty()) {
        const bool parent_exists = std::filesystem::exists(parent, error);
        if (error || (parent_exists && !std::filesystem::is_directory(parent, error)) || error)
            return fail("Cannot access the local profile directory.");
    }
    // An existing keybind file is not permission. Read only the separate choice.
    const auto decision_bytes = std::filesystem::file_size(consent_path(), error);
    const bool bounded_choice = !error && decision_bytes <= 16;
    error.clear();
    std::ifstream saved_choice;
    if (bounded_choice) saved_choice.open(consent_path(), std::ios::binary);
    std::string decision;
    if (saved_choice) std::getline(saved_choice, decision);
    saved_choice.close(); // Release the Windows handle before a remembered denial rewrites it.
    if (decision == "accepted") choice = consent::accepted;
    if (decision == "declined") choice = consent::declined;
    if (choice == consent::declined) return set_storage_consent(false);
    if (choice != consent::accepted) {
        message = choice == consent::pending ? "Choose whether to save keybinds locally." : "Keybinds are used for this session only.";
        return true;
    }
    return load_profile();
}

namespace {
bool load_profile()
{
    std::error_code error;
    const bool exists = std::filesystem::exists(destination, error);
    if (error) return fail("Cannot access the local keybind profile.");
    if (!exists) return true;
    const auto bytes = std::filesystem::file_size(destination, error);
    if (error || bytes > 96 * 1024) return fail("Invalid local keybind profile; existing file preserved.");
    std::ifstream input(destination, std::ios::binary);
    if (!input) return fail("Cannot read the local keybind profile.");
    const std::string payload((std::istreambuf_iterator<char>(input)), {});
    if (input.bad() || !modules::apply_keybind_config(payload, message)) {
        observed = modules::keybind_revision.load(std::memory_order_acquire);
        return fail("Invalid local keybind profile; existing file preserved.");
    }
    observed = modules::keybind_revision.load(std::memory_order_acquire);
    return true;
}
}

configs::keybind_profile::consent configs::keybind_profile::storage_consent() { return choice; }

bool configs::keybind_profile::set_storage_consent(bool accepted)
{
    const bool edited = modules::keybind_revision.load(std::memory_order_acquire) != observed || dirty;
    choice = accepted ? consent::accepted : consent::declined;
    dirty = false;
    observed = modules::keybind_revision.load(std::memory_order_acquire);
    success = true;
    if (destination.empty()) return fail("Local app data is unavailable; the preference cannot be saved.");
    std::error_code error;
    auto temporary = destination; temporary += ".tmp";
    if (!accepted) {
        // Revoke the persisted grant even if deleting a profile later fails.
        std::filesystem::remove(consent_path(), error);
        if (error) return fail("Cannot revoke the saved keybind preference.");
        // Disable writes before touching disk. Never delete a directory.
        for (const auto& path : {destination, temporary}) {
            if (std::filesystem::is_directory(path, error)) return fail("Cannot remove saved keybinds: the path is a directory.");
            error.clear(); std::filesystem::remove(path, error);
            if (error) return fail("Cannot remove the previously saved keybinds.");
        }
    }
    if (!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path(), error);
    if (error) return fail("Cannot save the local keybind preference.");
    auto marker = consent_path(); marker += ".tmp";
    {
        std::ofstream output(marker, std::ios::binary | std::ios::trunc);
        output << (accepted ? "accepted\n" : "declined\n"); output.close();
        if (!output) return fail("Cannot save the local keybind preference.");
    }
#ifdef _WIN32
    const bool replaced = MoveFileExW(marker.c_str(), consent_path().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    std::filesystem::rename(marker, consent_path(), error);
    const bool replaced = !error;
#endif
    if (!replaced) { std::filesystem::remove(marker, error); return fail("Cannot save the local keybind preference."); }
    if (accepted) {
        if (!edited && !load_profile()) return false;
        dirty = true;
        return flush(true);
    }
    message = "Keybinds are used for this session only; saved bindings removed.";
    return true;
}

bool configs::keybind_profile::flush(bool force)
{
    const auto revision = modules::keybind_revision.load(std::memory_order_acquire);
    if (choice != consent::accepted) {
        dirty = false;
        // Keep the original revision while permission is pending, so accepting
        // cannot overwrite bindings the user has already edited this session.
        return success;
    }
    const auto now = std::chrono::steady_clock::now();
    if (revision != observed) { observed = revision; dirty = true; changed_at = now; }
    if (!dirty) return success;
    if (!force && now - changed_at < std::chrono::milliseconds(400)) return success;
    changed_at = now; // Bound retries after an I/O failure.
    if (destination.empty()) return fail("Local app data is unavailable; keybinds cannot be saved.");
    std::error_code error;
    if (!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path(), error);
    if (error) return fail("Cannot create the local profile directory.");
    auto temporary = destination; temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return fail("Cannot write the local keybind profile.");
        const auto payload = modules::export_keybind_config();
        output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        output.close();
        if (output.fail()) { std::filesystem::remove(temporary, error); return fail("Keybind save failed; previous profile preserved."); }
    }
#ifdef _WIN32
    const bool replaced = MoveFileExW(temporary.c_str(), destination.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    std::filesystem::rename(temporary, destination, error);
    const bool replaced = !error;
#endif
    if (!replaced) { std::filesystem::remove(temporary, error); return fail("Keybind save failed; previous profile preserved."); }
    dirty = false; success = true; message = "Keybinds saved locally.";
    return true;
}

const std::string& configs::keybind_profile::status() { return message; }
bool configs::keybind_profile::healthy() { return success; }
