#include "../src/auth/device_proof.hpp"
#include <cassert>
#include <iostream>

int main()
{
    const auto a = auth::device::sign_request("activate", std::string(64, 'a'), std::string(32, 'b'),
        1800000000000LL, "TEST-LICENSE");
    if (!a.success) { std::cerr << a.message << '\n'; return 1; }
    const auto b = auth::device::sign_request("activate", std::string(64, 'a'), std::string(32, 'b'),
        1800000000000LL, "TEST-LICENSE");
    assert(b.success && a.public_key == b.public_key);
    // All fields are public test data; the private key never leaves Windows.
    std::cout << "{\"key\":\"TEST-LICENSE\",\"hwid\":\"" << std::string(64, 'a') <<
        "\",\"nonce\":\"" << std::string(32, 'b') <<
        "\",\"timestamp\":1800000000000,\"request_type\":\"activate\",\"device_key\":\"" <<
        a.public_key << "\",\"device_signature\":\"" << a.signature <<
        "\",\"hardware_backed\":" << (a.hardware_backed ? "true" : "false") << "}\n";
}
