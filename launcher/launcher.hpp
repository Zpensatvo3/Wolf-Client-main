#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <filesystem>

namespace launcher {
    struct release { std::string version, sha256, minecraft_target; uint64_t bytes = 0; std::filesystem::path path; };
    struct game { DWORD pid = 0; std::wstring title; };
    using progress = std::function<void(uint64_t, uint64_t)>;
    std::wstring wide(const std::string& value);
    std::string narrow(const std::wstring& value);
    release manifest();
    void download(release& item, std::atomic_bool& cancel, const progress& report);
    std::vector<game> games();
    void launch(const game& selected, const release& item, std::atomic_bool& cancel,
        const std::function<void(const std::wstring&)>& status);
}
