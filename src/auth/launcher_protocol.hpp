#pragma once
#include <Windows.h>
#include <bcrypt.h>
#include <sddl.h>
#include <string>
#include <vector>
#include <array>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")

namespace auth::launch {
    inline std::wstring pipe_name(DWORD pid) { return L"\\\\.\\pipe\\Onestep.Launch." + std::to_wstring(pid); }
    inline void wipe(std::string& text) { if (!text.empty()) SecureZeroMemory(text.data(), text.size()); text.clear(); }
    inline bool hex64(const std::string& s) {
        return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
    }
    inline std::string file_hash(const std::wstring& path) {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file == INVALID_HANDLE_VALUE) return {};
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(file, &info) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            CloseHandle(file); return {};
        }
        BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{};
        std::string result;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
            BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
            std::array<unsigned char, 65536> buffer{};
            DWORD count{}; bool ok = true;
            for (;;) {
                if (!ReadFile(file, buffer.data(), DWORD(buffer.size()), &count, nullptr)) { ok = false; break; }
                if (!count) break;
                if (BCryptHashData(hash, buffer.data(), count, 0) < 0) { ok = false; break; }
            }
            std::array<unsigned char, 32> digest{};
            if (ok && BCryptFinishHash(hash, digest.data(), DWORD(digest.size()), 0) >= 0) {
                constexpr char digits[] = "0123456789abcdef";
                for (auto c : digest) { result += digits[c >> 4]; result += digits[c & 15]; }
            }
        }
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        CloseHandle(file); return result;
    }
    inline PSECURITY_DESCRIPTOR user_security() {
        HANDLE token{};
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return nullptr;
        DWORD size{}; GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        std::vector<unsigned char> data(size);
        if (!GetTokenInformation(token, TokenUser, data.data(), size, &size)) { CloseHandle(token); return nullptr; }
        LPWSTR sid{}; PSECURITY_DESCRIPTOR descriptor{};
        if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid, &sid)) {
            const std::wstring sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;" + std::wstring(sid) + L")";
            ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr);
            LocalFree(sid);
        }
        CloseHandle(token); return descriptor;
    }
    inline std::string receive_ticket() {
        const auto name = pipe_name(GetCurrentProcessId());
        // Launcher creates the pipe before loading the DLL. No unattended polling.
        HANDLE pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) return {};
        DWORD mode=PIPE_READMODE_MESSAGE;
        if(!SetNamedPipeHandleState(pipe,&mode,nullptr,nullptr)){CloseHandle(pipe);return {};}
        char buffer[1024]{}; DWORD read{}; OVERLAPPED operation{};
        operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        bool ok = false;
        if (operation.hEvent) {
            BOOL immediate = ReadFile(pipe, buffer, sizeof(buffer), &read, &operation);
            if (immediate) ok = read > 64 && read < sizeof(buffer);
            else if (GetLastError() == ERROR_IO_PENDING && WaitForSingleObject(operation.hEvent, 10000) == WAIT_OBJECT_0)
                ok = GetOverlappedResult(pipe, &operation, &read, FALSE) && read > 64 && read < sizeof(buffer);
            if (!ok) { CancelIoEx(pipe, &operation); GetOverlappedResult(pipe, &operation, &read, TRUE); }
            CloseHandle(operation.hEvent);
        }
        CloseHandle(pipe);
        std::string ticket(buffer, ok ? read : 0);
        SecureZeroMemory(buffer, sizeof(buffer));
        return ticket;
    }
    inline std::wstring own_module_path() {
        HMODULE module{}; wchar_t path[32768]{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&own_module_path), &module)) return {};
        const auto size = GetModuleFileNameW(module, path, DWORD(std::size(path)));
        return size && size < std::size(path) ? std::wstring(path, size) : std::wstring{};
    }
}
