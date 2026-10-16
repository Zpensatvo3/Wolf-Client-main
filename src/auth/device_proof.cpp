#include "device_proof.hpp"
#include <Windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <wincrypt.h>
#include <array>
#include <cstring>
#include <mutex>
#include <vector>

#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace
{
    constexpr wchar_t key_name[] = L"Onestep.Auth.Device.V1";
    std::mutex key_mutex;

    struct handle
    {
        NCRYPT_HANDLE value = 0;
        ~handle() { if (value) NCryptFreeObject(value); }
        handle() = default;
        handle(const handle&) = delete;
        handle& operator=(const handle&) = delete;
    };

    bool digest(std::string_view text, std::array<unsigned char, 32>& output)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
        const auto result = BCryptHash(algorithm, nullptr, 0,
            reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())), static_cast<ULONG>(text.size()),
            output.data(), static_cast<ULONG>(output.size()));
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return result >= 0;
    }

    std::string hex(const std::array<unsigned char, 32>& value)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string result;
        for (const auto byte : value) { result += digits[byte >> 4]; result += digits[byte & 15]; }
        return result;
    }

    std::string encode(const unsigned char* data, DWORD length)
    {
        DWORD count = 0;
        const DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
        if (!CryptBinaryToStringA(data, length, flags, nullptr, &count)) return {};
        std::string output(count, '\0');
        if (!CryptBinaryToStringA(data, length, flags, output.data(), &count)) return {};
        while (!output.empty() && (output.back() == '\0' || output.back() == '=')) output.pop_back();
        for (char& c : output) { if (c == '+') c = '-'; else if (c == '/') c = '_'; }
        return output;
    }

    bool missing(SECURITY_STATUS status)
    {
        return status == NTE_BAD_KEYSET || status == NTE_NOT_FOUND || status == NTE_NO_KEY;
    }

    bool finish_proof(NCRYPT_KEY_HANDLE key, bool hardware,
        const std::array<unsigned char, 32>& hash, auth::device::proof& result)
    {
        DWORD export_policy = 0, property_size = 0;
        if (NCryptGetProperty(key, NCRYPT_EXPORT_POLICY_PROPERTY, reinterpret_cast<PBYTE>(&export_policy),
            sizeof(export_policy), &property_size, 0) != ERROR_SUCCESS ||
            property_size != sizeof(export_policy) || export_policy != 0) return false;
        std::array<unsigned char, sizeof(BCRYPT_ECCKEY_BLOB) + 64> blob{};
        DWORD size = 0;
        if (NCryptExportKey(key, 0, BCRYPT_ECCPUBLIC_BLOB, nullptr, blob.data(),
            static_cast<DWORD>(blob.size()), &size, 0) != ERROR_SUCCESS || size != blob.size()) return false;
        BCRYPT_ECCKEY_BLOB header{};
        std::memcpy(&header, blob.data(), sizeof(header));
        if (header.dwMagic != BCRYPT_ECDSA_PUBLIC_P256_MAGIC || header.cbKey != 32) return false;
        std::array<unsigned char, 65> point{};
        point[0] = 4;
        std::memcpy(point.data() + 1, blob.data() + sizeof(header), 64);
        std::array<unsigned char, 64> signature{};
        if (NCryptSignHash(key, nullptr, const_cast<PBYTE>(hash.data()), static_cast<DWORD>(hash.size()),
            signature.data(), static_cast<DWORD>(signature.size()), &size, NCRYPT_SILENT_FLAG) != ERROR_SUCCESS ||
            size != signature.size()) return false;
        result.public_key = encode(point.data(), static_cast<DWORD>(point.size()));
        result.signature = encode(signature.data(), static_cast<DWORD>(signature.size()));
        result.hardware_backed = hardware;
        result.success = !result.public_key.empty() && !result.signature.empty();
        return result.success;
    }
}

__declspec(noinline) auth::device::proof auth::device::sign_request(std::string_view type,
    std::string_view hwid, std::string_view nonce, long long timestamp, std::string_view credential)
{
    proof result;
    if ((type != "activate" && type != "heartbeat" && type != "redeem") || hwid.size() != 64 || nonce.size() != 32 ||
        credential.empty() || credential.size() > 16 * 1024 || timestamp <= 0)
    { result.message = "Invalid device verification request."; return result; }
    std::array<unsigned char, 32> credential_hash{}, request_hash{};
    if (!digest(credential, credential_hash)) { result.message = "Device request hashing failed."; return result; }
    const std::string message = "onestep-device-v1\n" + std::string(type) + "\n" + std::string(hwid) +
        "\n" + std::string(nonce) + "\n" + std::to_string(timestamp) + "\n" + hex(credential_hash);
    SecureZeroMemory(credential_hash.data(), credential_hash.size());
    if (!digest(message, request_hash)) { result.message = "Device request hashing failed."; return result; }

    std::lock_guard lock(key_mutex);
    const wchar_t* providers[] = { MS_PLATFORM_CRYPTO_PROVIDER, MS_KEY_STORAGE_PROVIDER };
    // Open both providers before creating anything. A previously selected
    // software key must not change just because a TPM becomes available later.
    for (const auto provider_name : providers)
    {
        handle provider, key;
        if (NCryptOpenStorageProvider(&provider.value, provider_name, 0) != ERROR_SUCCESS) continue;
        const auto status = NCryptOpenKey(provider.value, &key.value, key_name, 0, NCRYPT_SILENT_FLAG);
        if (status == ERROR_SUCCESS)
        {
            if (!finish_proof(key.value, provider_name == providers[0], request_hash, result))
                result.message = "The existing device key could not sign the request.";
            return result;
        }
        if (!missing(status)) { result.message = "The existing device key is unavailable."; return result; }
    }
    for (const auto provider_name : providers)
    {
        handle provider, key;
        if (NCryptOpenStorageProvider(&provider.value, provider_name, 0) != ERROR_SUCCESS) continue;
        // No overwrite flag. A concurrent process may have created the key.
        const auto status = NCryptCreatePersistedKey(provider.value, &key.value,
            NCRYPT_ECDSA_P256_ALGORITHM, key_name, 0, 0);
        if (status == NTE_EXISTS)
        {
            if (NCryptOpenKey(provider.value, &key.value, key_name, 0, NCRYPT_SILENT_FLAG) == ERROR_SUCCESS &&
                finish_proof(key.value, provider_name == providers[0], request_hash, result)) return result;
            result.message = "The device key was created by another process; please retry.";
            return result;
        }
        if (status != ERROR_SUCCESS) continue;
        DWORD exports = 0, usage = NCRYPT_ALLOW_SIGNING_FLAG;
        if (NCryptSetProperty(key.value, NCRYPT_EXPORT_POLICY_PROPERTY, reinterpret_cast<PBYTE>(&exports), sizeof(exports), 0) != ERROR_SUCCESS ||
            NCryptSetProperty(key.value, NCRYPT_KEY_USAGE_PROPERTY, reinterpret_cast<PBYTE>(&usage), sizeof(usage), 0) != ERROR_SUCCESS ||
            NCryptFinalizeKey(key.value, NCRYPT_SILENT_FLAG) != ERROR_SUCCESS) continue;
        if (finish_proof(key.value, provider_name == providers[0], request_hash, result)) return result;
        result.message = "The new device key could not sign the request.";
        return result;
    }
    result.message = "Windows could not create a protected device key.";
    return result;
}
