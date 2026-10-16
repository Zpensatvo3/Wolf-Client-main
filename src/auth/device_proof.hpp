#pragma once
#include <string>
#include <string_view>

namespace auth::device
{
    struct proof
    {
        bool success = false;
        bool hardware_backed = false;
        std::string public_key;
        std::string signature;
        std::string message;
    };

    // Uses a persistent, non-exportable Windows user key. Only its public
    // point and a signature leave CNG. Existing keys are never overwritten.
    proof sign_request(std::string_view type, std::string_view hwid,
        std::string_view nonce, long long timestamp, std::string_view credential);
}
