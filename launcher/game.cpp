#include "launcher.hpp"
#include "../src/auth/auth.hpp"
#include "../src/auth/launcher_protocol.hpp"
#include <tlhelp32.h>
#include <algorithm>
#include <stdexcept>
#include <memory>

namespace {
    struct handle {
        HANDLE value{};
        explicit handle(HANDLE h = nullptr) : value(h) {}
        ~handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
        handle(const handle&) = delete;
    };
    void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
    bool same_user(HANDLE process) {
        handle ours, theirs;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &ours.value) ||
            !OpenProcessToken(process, TOKEN_QUERY, &theirs.value)) return false;
        auto sid = [](HANDLE token) {
            DWORD size{}; GetTokenInformation(token, TokenUser, nullptr, 0, &size);
            std::vector<unsigned char> value(size);
            if (!GetTokenInformation(token, TokenUser, value.data(), size, &size)) value.clear();
            return value;
        };
        auto a = sid(ours.value), b = sid(theirs.value);
        return !a.empty() && !b.empty() && EqualSid(reinterpret_cast<TOKEN_USER*>(a.data())->User.Sid,
            reinterpret_cast<TOKEN_USER*>(b.data())->User.Sid);
    }
    std::vector<MODULEENTRY32W> modules(DWORD pid) {
        std::vector<MODULEENTRY32W> result;
        handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
        if (snapshot.value == INVALID_HANDLE_VALUE) return result;
        MODULEENTRY32W entry{}; entry.dwSize = sizeof(entry);
        if (Module32FirstW(snapshot.value, &entry)) do { result.push_back(entry); } while (Module32NextW(snapshot.value, &entry));
        return result;
    }
    bool minecraft(HWND window, launcher::game& result) {
        if (!IsWindowVisible(window)) return false;
        wchar_t title[512]{}, cls[128]{};
        GetWindowTextW(window, title, 512); GetClassNameW(window, cls, 128);
        if (std::wstring(title).find(L"Minecraft") == std::wstring::npos ||
            (std::wstring(cls) != L"GLFW30" && std::wstring(cls) != L"LWJGL")) return false;
        DWORD pid{}; GetWindowThreadProcessId(window, &pid);
        handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        if (!process.value || !same_user(process.value)) return false;
        USHORT machine{}, native{};
        if (!IsWow64Process2(process.value, &machine, &native) || machine != IMAGE_FILE_MACHINE_UNKNOWN ||
            native != IMAGE_FILE_MACHINE_AMD64) return false;
        wchar_t path[32768]{}; DWORD size = 32768;
        if (!QueryFullProcessImageNameW(process.value, 0, path, &size)) return false;
        const auto name = std::filesystem::path(path).filename().wstring();
        if (_wcsicmp(name.c_str(), L"javaw.exe") && _wcsicmp(name.c_str(), L"java.exe")) return false;
        const auto list = modules(pid);
        if (std::none_of(list.begin(), list.end(), [](auto& item) { return !_wcsicmp(item.szModule, L"jvm.dll"); })) return false;
        result = {pid, title}; return true;
    }
    bool wait(HANDLE object, DWORD milliseconds, std::atomic_bool& cancel) {
        const auto end = GetTickCount64() + milliseconds;
        while (!cancel.load() && GetTickCount64() < end) {
            const auto result = WaitForSingleObject(object, 100);
            if (result == WAIT_OBJECT_0) return true;
            if (result == WAIT_FAILED) return false;
        }
        return false;
    }
    struct pending_io {
        HANDLE pipe; OVERLAPPED operation{};
        explicit pending_io(HANDLE h) : pipe(h) { operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
        ~pending_io() {
            if (operation.hEvent) {
                CancelIoEx(pipe, &operation); DWORD bytes{};
                GetOverlappedResult(pipe, &operation, &bytes, TRUE);
                CloseHandle(operation.hEvent);
            }
        }
    };
}
std::vector<launcher::game> launcher::games() {
    std::vector<game> result;
    EnumWindows([](HWND window, LPARAM data) -> BOOL {
        auto& list = *reinterpret_cast<std::vector<game>*>(data); game found;
        if (minecraft(window, found) && std::none_of(list.begin(), list.end(), [&](auto& g) { return g.pid == found.pid; }))
            list.push_back(found);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&result));
    return result;
}
void launcher::launch(const game& selected, const release& item, std::atomic_bool& cancel,
    const std::function<void(const std::wstring&)>& status) {
    const auto current = games();
    require(std::any_of(current.begin(), current.end(), [&](auto& g) { return g.pid == selected.pid; }),
        "Seçilen Minecraft kapandı. Listeyi yenile.");
    require(auth::permitted(auth::scope::launcher), "Oturum sona erdi. Tekrar giriş yap.");
    // Keep the verified file locked against writes/replacement until loading finishes.
    handle lock(CreateFileW(item.path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
    require(lock.value != INVALID_HANDLE_VALUE, "Client dosyası açılamadı.");
    require(auth::launch::file_hash(item.path.wstring()) == item.sha256, "Client dosyası değişmiş. Tekrar giriş yap.");
    const auto loaded = modules(selected.pid);
    require(std::none_of(loaded.begin(), loaded.end(), [](auto& m) { return !_wcsicmp(m.szModule, L"Onestep.dll"); }),
        "Client zaten bu oyunda açık. İkinci kez yüklenmedi.");
    handle process(OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, selected.pid));
    require(process.value && same_user(process.value), "Minecraft'a erişilemiyor. Launcher ve oyunu aynı kullanıcıyla aç.");
    auto descriptor = auth::launch::user_security();
    require(descriptor != nullptr, "Güvenli yerel bağlantı oluşturulamadı.");
    std::unique_ptr<void, decltype(&LocalFree)> sd(descriptor, LocalFree);
    SECURITY_ATTRIBUTES security{sizeof(security), descriptor, FALSE};
    handle pipe(CreateNamedPipeW(auth::launch::pipe_name(selected.pid).c_str(), PIPE_ACCESS_DUPLEX |
        FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
        PIPE_REJECT_REMOTE_CLIENTS, 1, 256, 256, 0, &security));
    require(pipe.value != INVALID_HANDLE_VALUE, "Bu oyun için başka bir başlatma işlemi var.");
    const auto eventName = L"Local\\Onestep.Launch.Ready." + std::to_wstring(selected.pid);
    handle ready(CreateEventW(&security, TRUE, FALSE, eventName.c_str()));
    require(ready.value && GetLastError() != ERROR_ALREADY_EXISTS, "Başlatma onayı hazırlanamadı. Biraz bekleyip tekrar dene.");
    pending_io connection(pipe.value);
    require(connection.operation.hEvent != nullptr, "Yerel bağlantı hazırlanamadı.");
    BOOL connected = ConnectNamedPipe(pipe.value, &connection.operation);
    const DWORD connectError = connected ? ERROR_SUCCESS : GetLastError();
    require(connected || connectError == ERROR_IO_PENDING || connectError == ERROR_PIPE_CONNECTED, "Yerel bağlantı açılamadı.");
    status(L"Başlatma izni alınıyor...");
    const auto process_nonce=auth::generate_nonce();
    require(process_nonce.size()==32,"Başlatma kimliği oluşturulamadı.");
    auto response = auth::post_authenticated_json("/api/v2/launcher/ticket", "\"pid\":" + std::to_string(selected.pid) +
        ",\"release_sha256\":\"" + item.sha256 + "\",\"release_version\":\"" + item.version +
        "\",\"minecraft_target\":\"" + item.minecraft_target + "\",\"process_nonce\":\"" + process_nonce + "\"");
    if (!response.success) {
        if (response.status_code == 401 || response.status_code == 403 || response.status_code == 409)
            auth::controlled_shutdown();
        throw std::runtime_error(response.message);
    }
    auto ticketEnvelope=auth::decode_service_payload(response.body,"onestep-launcher-v2");
    auto ticket = auth::service_field(ticketEnvelope, "ticket");
    auth::launch::wipe(ticketEnvelope);
    auth::launch::wipe(response.body);
    struct ticket_guard { std::string& value; ~ticket_guard() { auth::launch::wipe(value); } } secret{ticket};
    require(auth::launch::hex64(ticket), "Başlatma izni geçersiz.");
    auto handoff="{\"ticket\":\""+ticket+"\",\"process_nonce\":\""+process_nonce+
        "\",\"release_version\":\""+item.version+"\",\"minecraft_target\":\""+item.minecraft_target+"\"}";
    struct handoff_guard {std::string& value;~handoff_guard(){auth::launch::wipe(value);}} wipe_handoff{handoff};
    if (cancel.load()) throw std::runtime_error("İşlem iptal edildi.");
    status(L"Client Minecraft'a yükleniyor...");
    // Standard Windows DLL loading only: no manual mapping, driver or stealth.
    const auto path = item.path.wstring(); const auto bytes = (path.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process.value, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    require(remote != nullptr, "Minecraft yükleme alanı hazırlanamadı.");
    bool inFlight = false;
    struct remote_guard { HANDLE process; void* address; bool& inFlight;
        ~remote_guard() { if (!inFlight) VirtualFreeEx(process, address, 0, MEM_RELEASE); } } allocation{process.value, remote, inFlight};
    SIZE_T written{};
    require(WriteProcessMemory(process.value, remote, path.c_str(), bytes, &written) && written == bytes,
        "Client yolu Minecraft'a iletilemedi.");
    const auto localLoad = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HMODULE owner{};
    require(localLoad && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(localLoad), &owner), "Windows yükleyicisi bulunamadı.");
    wchar_t ownerPath[32768]{}; GetModuleFileNameW(owner, ownerPath, 32768);
    const auto ownerName = std::filesystem::path(ownerPath).filename().wstring();
    const auto targetOwner = std::find_if(loaded.begin(), loaded.end(), [&](auto& m) { return !_wcsicmp(m.szModule, ownerName.c_str()); });
    require(targetOwner != loaded.end(), "Minecraft Windows yükleyicisi bulunamadı.");
    const auto offset = reinterpret_cast<uintptr_t>(localLoad) - reinterpret_cast<uintptr_t>(owner);
    require(offset < targetOwner->modBaseSize, "Windows yükleyici adresi doğrulanamadı.");
    const auto entry = reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<uintptr_t>(targetOwner->modBaseAddr) + offset);
    handle thread(CreateRemoteThread(process.value, nullptr, 0, entry, remote, 0, nullptr));
    require(thread.value != nullptr, "Windows client yüklemesini başlatamadı.");
    inFlight = true;
    require(wait(thread.value, 15000, cancel), "Yükleme onayı gecikti. Oyunu kapatma; tekrar yüklemeden önce kontrol et.");
    inFlight = false;
    const auto after = modules(selected.pid);
    require(std::any_of(after.begin(), after.end(), [&](auto& m) { return !_wcsicmp(m.szExePath, path.c_str()); }),
        "Client yüklenemedi. Fabric 1.21.11 ve Windows x64 kullan.");
    status(L"Client oturumu doğrulanıyor...");
    if (!connected && connectError == ERROR_IO_PENDING)
        require(wait(connection.operation.hEvent, 45000, cancel), "Client bağlantısı kurulamadı. Oyundaki hata mesajını kontrol et.");
    ULONG clientPid{};
    require(GetNamedPipeClientProcessId(pipe.value, &clientPid) && clientPid == selected.pid, "Yanlış işlem bağlantısı reddedildi.");
    pending_io write(pipe.value); DWORD sent{};
    BOOL immediate = WriteFile(pipe.value, handoff.data(), DWORD(handoff.size()), &sent, &write.operation);
    if (!immediate) {
        require(GetLastError() == ERROR_IO_PENDING && wait(write.operation.hEvent, 10000, cancel) &&
            GetOverlappedResult(pipe.value, &write.operation, &sent, FALSE), "Başlatma izni iletilemedi.");
    }
    require(sent == handoff.size(), "Başlatma izni eksik iletildi.");
    auth::launch::wipe(ticket);
    require(wait(ready.value, 45000, cancel), "Client oturumu onaylanmadı. Oyundaki giriş durumunu kontrol et.");
    status(L"Client hazır. İyi oyunlar.");
}
