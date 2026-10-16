// Operator-run probe only. Uses a temporary license supplied on stdin.
// Does not open, close, inject into, or modify Minecraft.
#include "../launcher/launcher.hpp"
#include "../src/auth/auth.hpp"
#include "../src/auth/config.hpp"
#include "../src/auth/launcher_protocol.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
int ipc_probe() {
    auto sd = auth::launch::user_security();
    if (!sd) return 1;
    SECURITY_ATTRIBUTES security{sizeof(security), sd, FALSE};
    HANDLE pipe = CreateNamedPipeW(auth::launch::pipe_name(GetCurrentProcessId()).c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 256, 256, 0, &security);
    LocalFree(sd);
    if (pipe == INVALID_HANDLE_VALUE) return 1;
    OVERLAPPED connection{}, write{};
    connection.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    write.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    bool ok = connection.hEvent && write.hEvent;
    BOOL connected = ok ? ConnectNamedPipe(pipe, &connection) : FALSE;
    DWORD error = connected ? ERROR_SUCCESS : GetLastError();
    std::string received;
    std::thread reader([&] { received = auth::launch::receive_ticket(); });
    ok = ok && (connected || error == ERROR_PIPE_CONNECTED ||
        (error == ERROR_IO_PENDING && WaitForSingleObject(connection.hEvent, 5000) == WAIT_OBJECT_0));
    DWORD sent{}; const std::string expected(64, 'a');
    if (ok) {
        BOOL immediate = WriteFile(pipe, expected.data(), 64, &sent, &write);
        ok = immediate || (GetLastError() == ERROR_IO_PENDING && WaitForSingleObject(write.hEvent, 5000) == WAIT_OBJECT_0 &&
            GetOverlappedResult(pipe, &write, &sent, FALSE));
        ok = ok && sent == 64;
    }
    CancelIoEx(pipe, nullptr);
    if (connection.hEvent) { DWORD ignored{}; GetOverlappedResult(pipe, &connection, &ignored, TRUE); CloseHandle(connection.hEvent); }
    if (write.hEvent) { DWORD ignored{}; GetOverlappedResult(pipe, &write, &ignored, TRUE); CloseHandle(write.hEvent); }
    reader.join(); CloseHandle(pipe);
    if (!ok || received != expected) return 1;
    std::cout << "PASS: user-restricted, local-only named-pipe ticket handoff\n"; return 0;
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--ipc-only") return ipc_probe();
    try {
        std::string key; if (!std::getline(std::cin, key)) return 2;
        auto login = auth::authenticate(key, auth::config::server_domain);
        auth::launch::wipe(key); auth::launch::wipe(login.session_token);
        if (!login.success) throw std::runtime_error(login.message);
        auto release = launcher::manifest(); std::atomic_bool cancel{false};
        launcher::download(release, cancel, [](auto, auto) {});
        if (auth::launch::file_hash(release.path.wstring()) != release.sha256) throw std::runtime_error("Download mismatch");
        auto issued = auth::post_authenticated_json("/api/v1/launcher/ticket", "\"pid\":" + std::to_string(GetCurrentProcessId()) +
            ",\"release_sha256\":\"" + release.sha256 + "\"");
        if (!issued.success) throw std::runtime_error(issued.message);
        auto ticket = auth::service_field(issued.body, "ticket"); auth::launch::wipe(issued.body);
        auto redeemed = auth::redeem_launcher_ticket(ticket, auth::config::server_domain, release.sha256);
        auth::launch::wipe(redeemed.session_token);
        if (!redeemed.success) throw std::runtime_error(redeemed.message);
        auto duplicate = auth::redeem_launcher_ticket(ticket, auth::config::server_domain, release.sha256);
        auth::launch::wipe(ticket); auth::launch::wipe(duplicate.session_token);
        if (duplicate.success) throw std::runtime_error("Ticket replay accepted");
        auth::controlled_shutdown();
        std::cout << "PASS: native login, signed manifest, private download, hash, one-use redemption, replay rejection\n";
        return 0;
    } catch (const std::exception& error) {
        auth::controlled_shutdown(); std::cerr << error.what() << '\n'; return 1;
    }
}
