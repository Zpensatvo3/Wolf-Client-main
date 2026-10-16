// Explicit operator-run smoke check. Never run as part of the offline suite.
#include "../src/auth/auth.hpp"
#include "../src/auth/device_proof.hpp"
#include <Windows.h>
#include <chrono>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::string key;
    if (!std::getline(std::cin, key)) return 2;
    auto invalid = auth::authenticate("ONESTEP-INVALID-SMOKE-KEY", argv[1]);
    if (invalid.success) { std::cerr << "Invalid key was accepted\n"; return 1; }
    auto result = auth::authenticate(key, argv[1]);
    SecureZeroMemory(key.data(), key.size());
    key.clear();
    if (!result.success)
    {
        std::cerr << "Native login failed: " << result.message << '\n';
        auth::controlled_shutdown();
        return 1;
    }
    const auto configs = auth::post_authenticated_json("/api/v1/configs/list", "");
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto hwid = auth::generate_hwid();
    const auto nonce = auth::generate_nonce();
    const auto proof = auth::device::sign_request("heartbeat", hwid, nonce, now, result.session_token);
    const std::string fields = "\"hwid\":\"" + hwid +
        "\",\"nonce\":\"" + nonce + "\",\"timestamp\":" + std::to_string(now) +
        ",\"request_type\":\"heartbeat\",\"device_key\":\"" + proof.public_key +
        "\",\"device_signature\":\"" + proof.signature + "\"";
    const auto heartbeat = auth::post_authenticated_json("/api/v1/auth", fields);
    const std::string legacy_fields = "\"hwid\":\"" + hwid + "\",\"nonce\":\"" + auth::generate_nonce() +
        "\",\"timestamp\":" + std::to_string(now) + ",\"request_type\":\"heartbeat\"";
    const auto unsigned_heartbeat = auth::post_authenticated_json("/api/v1/auth", legacy_fields);
    SecureZeroMemory(result.session_token.data(), result.session_token.size());
    result.session_token.clear();
    auth::controlled_shutdown();
    if (!configs.success || !heartbeat.success || unsigned_heartbeat.status_code != 403)
    {
        std::cerr << "Config/heartbeat check failed (" << configs.status_code << ", " << heartbeat.status_code << ")\n";
        return 1;
    }
    std::cout << "Native device-bound login, signed payload, licensed configs, signed heartbeat and unsigned-heartbeat rejection: PASS\n";
}
