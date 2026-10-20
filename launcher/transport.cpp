#include "launcher.hpp"
#include "../src/auth/auth.hpp"
#include "../src/auth/config.hpp"
#include "../src/auth/launcher_protocol.hpp"
#include <winhttp.h>
#include <shlobj.h>
#include <fstream>
#include <charconv>
#include <stdexcept>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace {
    struct http { std::atomic<HINTERNET> value{}; ~http() { if(auto h=value.exchange(nullptr)) WinHttpCloseHandle(h); } };
    struct file_handle { HANDLE value = INVALID_HANDLE_VALUE; ~file_handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); } };
    void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}
std::wstring launcher::wide(const std::string& value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), int(value.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    if (size) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), int(value.size()), result.data(), size);
    return result;
}
std::string launcher::narrow(const std::wstring& value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), int(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    if (size) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), int(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}
launcher::release launcher::manifest() {
    const auto response = auth::post_authenticated_json("/api/v2/launcher/manifest", "\"minecraft_target\":\"1.21.11\"");
    require(response.success, response.message.c_str());
    const auto payload = auth::decode_service_payload(response.body, "onestep-release-v2");
    require(!payload.empty(), "Güncelleme imzası doğrulanamadı.");
    release result;
    result.version = auth::service_field(payload, "version");
    result.sha256 = auth::service_field(payload, "sha256");
    result.minecraft_target = auth::service_field(payload, "minecraft_target");
    const auto bytes = auth::service_field(payload, "bytes");
    const auto parsed = std::from_chars(bytes.data(), bytes.data() + bytes.size(), result.bytes);
    require(parsed.ec == std::errc{} && parsed.ptr == bytes.data() + bytes.size() &&
        result.bytes >= 4096 && result.bytes <= 24 * 1024 * 1024 && auth::launch::hex64(result.sha256) &&
        !result.version.empty() && result.version.size() <= 64 && result.minecraft_target == "1.21.11", "Güncelleme bilgileri geçersiz.");
    return result;
}
void launcher::download(release& item, std::atomic_bool& cancel, const progress& report) {
    PWSTR local{};
    require(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local)), "Yerel klasör açılamadı.");
    std::filesystem::path directory = std::filesystem::path(local) / L"Onestep" / L"releases" / wide(item.sha256);
    CoTaskMemFree(local);
    std::filesystem::create_directories(directory);
    item.path = directory / L"Onestep.dll";
    if (std::filesystem::exists(item.path) && std::filesystem::file_size(item.path) == item.bytes &&
        auth::launch::file_hash(item.path.wstring()) == item.sha256) { report(item.bytes, item.bytes); return; }
    const auto temporary = directory / (L"download-" + wide(auth::generate_nonce()) + L".part");
    try {
        http session{WinHttpOpen(L"Onestep-Launcher/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
        require(session.value != nullptr, "İnternet bağlantısı başlatılamadı.");
        WinHttpSetTimeouts(session.value, 5000, 5000, 5000, 5000);
        const auto domain = wide(auth::config::server_domain);
        http connection{WinHttpConnect(session.value, domain.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0)};
        require(connection.value != nullptr, "Sunucuya bağlanılamadı.");
        http request{WinHttpOpenRequest(connection.value, L"POST", L"/api/v2/launcher/download", nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
        require(request.value != nullptr, "İndirme başlatılamadı.");
        struct deadline_guard {
            http& request;std::mutex mutex;std::condition_variable cv;bool done=false;std::thread timer;
            deadline_guard(http& h):request(h),timer([this]{std::unique_lock lock(mutex);
                if(!cv.wait_for(lock,std::chrono::seconds(5),[this]{return done;}))
                    if(auto handle=request.value.exchange(nullptr))WinHttpCloseHandle(handle);}){}
            ~deadline_guard(){{std::lock_guard lock(mutex);done=true;}cv.notify_all();timer.join();}
        } deadline(request);
        DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        require(WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)) != FALSE,
            "Güvenli bağlantı kurulamadı.");
        auto body = auth::make_launcher_request("/api/v2/launcher/download", "\"release_sha256\":\"" + item.sha256 +
            "\",\"release_version\":\"" + item.version + "\",\"minecraft_target\":\"" + item.minecraft_target + "\"");
        require(!body.empty(), "Oturum sona erdi. Tekrar giriş yap.");
        BOOL sent = WinHttpSendRequest(request.value, L"Content-Type: application/json\r\nCache-Control: no-store\r\n",
            DWORD(-1), body.data(), DWORD(body.size()), DWORD(body.size()), 0);
        auth::launch::wipe(body);
        require(sent && WinHttpReceiveResponse(request.value, nullptr), "İndirme bağlantısı kesildi. Tekrar dene.");
        DWORD status{}, size = sizeof(status);
        require(WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            nullptr, &status, &size, nullptr) && status == 200, "İndirme reddedildi veya sürüm henüz hazır değil. Tekrar giriş yap.");
        file_handle file{CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
        require(file.value != INVALID_HANDLE_VALUE, "İndirme dosyası oluşturulamadı.");
        std::array<unsigned char, 65536> buffer{};
        uint64_t total{};
        for (;;) {
            if (cancel.load()) throw std::runtime_error("İşlem iptal edildi.");
            DWORD read{}, written{};
            require(WinHttpReadData(request.value, buffer.data(), DWORD(buffer.size()), &read) != FALSE, "İndirme tamamlanamadı.");
            if (!read) break;
            total += read;
            require(total <= item.bytes, "İndirilen dosyanın boyutu geçersiz.");
            require(WriteFile(file.value, buffer.data(), read, &written, nullptr) && written == read, "Dosya kaydedilemedi.");
            report(total, item.bytes);
        }
        require(total == item.bytes && FlushFileBuffers(file.value), "Dosya eksik indirildi.");
        CloseHandle(file.value); file.value = INVALID_HANDLE_VALUE;
        require(auth::launch::file_hash(temporary.wstring()) == item.sha256, "Dosya doğrulanamadı; çalıştırılmadı.");
        require(MoveFileExW(temporary.c_str(), item.path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE,
            "Güncelleme yerleştirilemedi. Client zaten açık olabilir.");
    } catch (...) {
        auth::controlled_shutdown();
        std::error_code ignored; std::filesystem::remove(temporary, ignored); throw;
    }
}
