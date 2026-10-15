#include "auth.hpp"

#include <Windows.h>
#include <bcrypt.h>
#include <intrin.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "Advapi32.lib")

// Set this at release-build time to the base64-encoded DER/SPKI public key
// paired with the Worker SIGNING_PRIVATE_KEY_PKCS8 secret. Keeping this public
// key in the binary allows the client to reject forged Worker responses without
// ever embedding a server secret.
#ifndef WOLF_AUTH_PUBLIC_KEY_SPKI_BASE64
#define WOLF_AUTH_PUBLIC_KEY_SPKI_BASE64 "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEMmmZ7wva7RAdQWJTPReMN3sEsh-VUBCOoEsGt4nSgDQMQXw0y1XUBdBNRn0UBUJQC1DEwQAIzDu-ziFvFFJDKQ"
#endif

namespace
{
	constexpr std::size_t kMaxResponseBytes = 1024 * 1024;
	// A temporary Cloudflare/ISP failure must never make a valid licence look
	// invalid.  We keep the local session alive while reconnecting and only
	// close it for an explicit, signed server-side rejection (ban, HWID reset,
	// epoch change, etc.).
	constexpr auto kHeartbeatRetryInitial = std::chrono::seconds(12);
	constexpr auto kHeartbeatRetryMaximum = std::chrono::seconds(90);
	constexpr char kAuthPublicKeySpkiBase64[] = WOLF_AUTH_PUBLIC_KEY_SPKI_BASE64;

	std::atomic<bool> g_authenticated{ false };
	std::atomic<int> g_active_epoch{ 1 };

	std::mutex g_token_mutex;
	std::string g_session_token;
	std::mutex g_hwid_mutex;
	std::string g_session_hwid;
	std::mutex g_service_domain_mutex;
	std::string g_service_domain;

	std::atomic<bool> g_heartbeat_running{ false };
	std::atomic<bool> g_heartbeat_exited{ true };
	std::mutex g_heartbeat_mutex;
	std::condition_variable g_heartbeat_cv;
	std::thread::id g_heartbeat_thread_id{};

	enum class failure_class
	{
		none,
		transient,
		hard
	};

	struct auth_attempt
	{
		auth::auth_result result;
		failure_class failure = failure_class::hard;
	};

	class winhttp_handle
	{
	public:
		winhttp_handle() = default;
		explicit winhttp_handle(HINTERNET value) noexcept : value_(value) {}

		~winhttp_handle()
		{
			reset();
		}

		winhttp_handle(const winhttp_handle&) = delete;
		winhttp_handle& operator=(const winhttp_handle&) = delete;

		winhttp_handle(winhttp_handle&& other) noexcept : value_(other.release()) {}
		winhttp_handle& operator=(winhttp_handle&& other) noexcept
		{
			if (this != &other)
			{
				reset(other.release());
			}
			return *this;
		}

		HINTERNET get() const noexcept { return value_; }
		explicit operator bool() const noexcept { return value_ != nullptr; }

		HINTERNET release() noexcept
		{
			HINTERNET old = value_;
			value_ = nullptr;
			return old;
		}

		void reset(HINTERNET replacement = nullptr) noexcept
		{
			if (value_)
				WinHttpCloseHandle(value_);
			value_ = replacement;
		}

	private:
		HINTERNET value_ = nullptr;
	};

	long long unix_time_ms()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}

	void secure_clear(std::string& value)
	{
		if (!value.empty())
		{
			SecureZeroMemory(value.data(), value.size());
			value.clear();
			value.shrink_to_fit();
		}
	}

	std::string bytes_to_hex(const UCHAR* data, std::size_t size)
	{
		static constexpr char hex[] = "0123456789abcdef";
		std::string output(size * 2, '\0');
		for (std::size_t i = 0; i < size; ++i)
		{
			output[i * 2] = hex[(data[i] >> 4) & 0x0F];
			output[i * 2 + 1] = hex[data[i] & 0x0F];
		}
		return output;
	}

	bool sha256_digest(
		std::string_view input,
		std::array<UCHAR, 32>& digest)
	{
		BCRYPT_ALG_HANDLE algorithm = nullptr;
		BCRYPT_HASH_HANDLE hash = nullptr;
		std::vector<UCHAR> hash_object;
		digest.fill(0);
		DWORD object_size = 0;
		DWORD bytes_written = 0;

		NTSTATUS status = BCryptOpenAlgorithmProvider(
			&algorithm,
			BCRYPT_SHA256_ALGORITHM,
			nullptr,
			0);
		if (status < 0)
			return false;

		status = BCryptGetProperty(
			algorithm,
			BCRYPT_OBJECT_LENGTH,
			reinterpret_cast<PUCHAR>(&object_size),
			sizeof(object_size),
			&bytes_written,
			0);
		if (status < 0 || object_size == 0)
		{
			BCryptCloseAlgorithmProvider(algorithm, 0);
			return false;
		}

		hash_object.resize(object_size);
		status = BCryptCreateHash(
			algorithm,
			&hash,
			hash_object.data(),
			static_cast<ULONG>(hash_object.size()),
			nullptr,
			0,
			0);

		if (status >= 0)
		{
			status = BCryptHashData(
				hash,
				reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
				static_cast<ULONG>(input.size()),
				0);
		}

		if (status >= 0)
		{
			status = BCryptFinishHash(
				hash,
				digest.data(),
				static_cast<ULONG>(digest.size()),
				0);
		}

		if (hash)
			BCryptDestroyHash(hash);
		BCryptCloseAlgorithmProvider(algorithm, 0);

		SecureZeroMemory(hash_object.data(), hash_object.size());
		if (status < 0)
		{
			digest.fill(0);
			return false;
		}

		return true;
	}

	std::string sha256_hex(std::string_view input)
	{
		std::array<UCHAR, 32> digest{};
		if (!sha256_digest(input, digest))
			return {};

		std::string result = bytes_to_hex(digest.data(), digest.size());
		SecureZeroMemory(digest.data(), digest.size());
		return result;
	}

	std::wstring utf8_to_wide(const std::string& value)
	{
		if (value.empty())
			return {};

		const int required = MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			value.data(),
			static_cast<int>(value.size()),
			nullptr,
			0);
		if (required <= 0)
			return {};

		std::wstring output(static_cast<std::size_t>(required), L'\0');
		if (MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			value.data(),
			static_cast<int>(value.size()),
			output.data(),
			required) != required)
		{
			return {};
		}
		return output;
	}

	std::string wide_to_utf8(const wchar_t* value)
	{
		if (!value || !*value)
			return {};

		const int required = WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			value,
			-1,
			nullptr,
			0,
			nullptr,
			nullptr);
		if (required <= 1)
			return {};

		std::string output(static_cast<std::size_t>(required), '\0');
		if (WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			value,
			-1,
			output.data(),
			required,
			nullptr,
			nullptr) <= 0)
		{
			return {};
		}
		output.pop_back(); // Remove the terminating NUL written by Win32.
		return output;
	}

	std::string read_machine_guid()
	{
		// A native DLL can be reached through either registry view depending on
		// how the JVM was launched.  Reading only one view made an occasional
		// registry access failure produce a different fallback fingerprint.
		constexpr DWORD views[] = {
			RRF_SUBKEY_WOW6464KEY,
			RRF_SUBKEY_WOW6432KEY,
			0
		};

		for (const DWORD view : views)
		{
			wchar_t buffer[256]{};
			DWORD bytes = sizeof(buffer);
			if (RegGetValueW(
				HKEY_LOCAL_MACHINE,
				L"SOFTWARE\\Microsoft\\Cryptography",
				L"MachineGuid",
				RRF_RT_REG_SZ | view,
				nullptr,
				buffer,
				&bytes) == ERROR_SUCCESS)
			{
				std::string guid = wide_to_utf8(buffer);
				std::transform(guid.begin(), guid.end(), guid.begin(), [](unsigned char ch) {
					return static_cast<char>(std::tolower(ch));
				});
				if (!guid.empty())
					return guid;
			}
		}
		return {};
	}

	std::string json_escape(std::string_view value)
	{
		std::string output;
		output.reserve(value.size() + 8);

		for (const unsigned char ch : value)
		{
			switch (ch)
			{
			case '"': output += "\\\""; break;
			case '\\': output += "\\\\"; break;
			case '\b': output += "\\b"; break;
			case '\f': output += "\\f"; break;
			case '\n': output += "\\n"; break;
			case '\r': output += "\\r"; break;
			case '\t': output += "\\t"; break;
			default:
				if (ch < 0x20)
				{
					std::ostringstream escaped;
					escaped << "\\u" << std::hex << std::setw(4)
						<< std::setfill('0') << static_cast<int>(ch);
					output += escaped.str();
				}
				else
				{
					output.push_back(static_cast<char>(ch));
				}
				break;
			}
		}
		return output;
	}

	void skip_json_whitespace(const std::string& json, std::size_t& position)
	{
		while (position < json.size() &&
			std::isspace(static_cast<unsigned char>(json[position])))
		{
			++position;
		}
	}

	std::optional<std::string> parse_json_value(
		const std::string& json,
		const std::string& key)
	{
		const std::string needle = "\"" + key + "\"";
		std::size_t position = json.find(needle);
		if (position == std::string::npos)
			return std::nullopt;

		position += needle.size();
		skip_json_whitespace(json, position);
		if (position >= json.size() || json[position] != ':')
			return std::nullopt;

		++position;
		skip_json_whitespace(json, position);
		if (position >= json.size())
			return std::nullopt;

		if (json[position] == '"')
		{
			++position;
			std::string output;
			while (position < json.size())
			{
				const char ch = json[position++];
				if (ch == '"')
					return output;
				if (ch != '\\')
				{
					output.push_back(ch);
					continue;
				}

				if (position >= json.size())
					return std::nullopt;

				const char escaped = json[position++];
				switch (escaped)
				{
				case '"': output.push_back('"'); break;
				case '\\': output.push_back('\\'); break;
				case '/': output.push_back('/'); break;
				case 'b': output.push_back('\b'); break;
				case 'f': output.push_back('\f'); break;
				case 'n': output.push_back('\n'); break;
				case 'r': output.push_back('\r'); break;
				case 't': output.push_back('\t'); break;
				default: return std::nullopt;
				}
			}
			return std::nullopt;
		}

		const std::size_t start = position;
		while (position < json.size() && json[position] != ',' && json[position] != '}')
			++position;

		std::size_t end = position;
		while (end > start &&
			std::isspace(static_cast<unsigned char>(json[end - 1])))
		{
			--end;
		}
		return json.substr(start, end - start);
	}

	bool parse_json_bool(const std::string& json, const std::string& key, bool fallback)
	{
		const auto value = parse_json_value(json, key);
		if (!value)
			return fallback;
		return *value == "true" || *value == "1";
	}

	template <typename Integer>
	std::optional<Integer> parse_integer(const std::optional<std::string>& value)
	{
		if (!value || value->empty())
			return std::nullopt;

		Integer parsed{};
		const char* begin = value->data();
		const char* end = begin + value->size();
		const auto result = std::from_chars(begin, end, parsed);
		if (result.ec != std::errc{} || result.ptr != end)
			return std::nullopt;
		return parsed;
	}

	bool constant_time_equal(std::string_view left, std::string_view right)
	{
		if (left.size() != right.size())
			return false;

		unsigned char difference = 0;
		for (std::size_t i = 0; i < left.size(); ++i)
			difference |= static_cast<unsigned char>(left[i] ^ right[i]);
		return difference == 0;
	}

	bool base64url_decode(std::string value, std::vector<UCHAR>& output)
	{
		if (value.empty() || value.size() > 16 * 1024)
			return false;

		for (char& ch : value)
		{
			if (ch == '-') ch = '+';
			else if (ch == '_') ch = '/';
			else if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '+' && ch != '/')
				return false;
		}
		while ((value.size() % 4) != 0)
			value.push_back('=');

		DWORD byte_count = 0;
		if (!CryptStringToBinaryA(value.c_str(), static_cast<DWORD>(value.size()),
			CRYPT_STRING_BASE64, nullptr, &byte_count, nullptr, nullptr) || byte_count == 0)
		{
			return false;
		}

		output.resize(byte_count);
		if (!CryptStringToBinaryA(value.c_str(), static_cast<DWORD>(value.size()),
			CRYPT_STRING_BASE64, output.data(), &byte_count, nullptr, nullptr))
		{
			SecureZeroMemory(output.data(), output.size());
			output.clear();
			return false;
		}
		output.resize(byte_count);
		return true;
	}

	bool verify_signed_payload(
		const std::string& payload,
		const std::string& signature_base64url)
	{
		if (kAuthPublicKeySpkiBase64[0] == '\0')
			return false;

		std::vector<UCHAR> public_key_der;
		std::vector<UCHAR> signature;
		if (!base64url_decode(kAuthPublicKeySpkiBase64, public_key_der) ||
			!base64url_decode(signature_base64url, signature))
		{
			return false;
		}

		CERT_PUBLIC_KEY_INFO* public_key_info = nullptr;
		DWORD public_key_info_size = 0;
		const BOOL decoded = CryptDecodeObjectEx(
			X509_ASN_ENCODING,
			X509_PUBLIC_KEY_INFO,
			public_key_der.data(),
			static_cast<DWORD>(public_key_der.size()),
			CRYPT_DECODE_ALLOC_FLAG,
			nullptr,
			&public_key_info,
			&public_key_info_size);
		SecureZeroMemory(public_key_der.data(), public_key_der.size());
		if (!decoded || !public_key_info)
		{
			SecureZeroMemory(signature.data(), signature.size());
			return false;
		}

		BCRYPT_KEY_HANDLE public_key = nullptr;
		const BOOL imported = CryptImportPublicKeyInfoEx2(
			X509_ASN_ENCODING,
			public_key_info,
			0,
			nullptr,
			&public_key);
		LocalFree(public_key_info);
		if (!imported || !public_key)
		{
			SecureZeroMemory(signature.data(), signature.size());
			return false;
		}

		// WebCrypto ECDSA hashes the payload with SHA-256 before signing. CNG's
		// BCryptVerifySignature expects that digest rather than the original text.
		std::array<UCHAR, 32> payload_digest{};
		if (!sha256_digest(payload, payload_digest))
		{
			BCryptDestroyKey(public_key);
			SecureZeroMemory(signature.data(), signature.size());
			return false;
		}

		const NTSTATUS status = BCryptVerifySignature(
			public_key,
			nullptr,
			payload_digest.data(),
			static_cast<ULONG>(payload_digest.size()),
			signature.data(),
			static_cast<ULONG>(signature.size()),
			0);
		BCryptDestroyKey(public_key);
		SecureZeroMemory(payload_digest.data(), payload_digest.size());
		SecureZeroMemory(signature.data(), signature.size());
		return status >= 0;
	}

	bool read_response_body(HINTERNET request, std::string& output)
	{
		output.clear();

		while (true)
		{
			DWORD available = 0;
			if (!WinHttpQueryDataAvailable(request, &available))
				return false;
			if (available == 0)
				return true;
			if (output.size() + available > kMaxResponseBytes)
				return false;

			std::vector<char> buffer(static_cast<std::size_t>(available));
			DWORD downloaded = 0;
			if (!WinHttpReadData(request, buffer.data(), available, &downloaded))
				return false;
			if (downloaded == 0)
				return true;

			output.append(buffer.data(), static_cast<std::size_t>(downloaded));
		}
	}

	DWORD query_http_status(HINTERNET request)
	{
		DWORD status_code = 0;
		DWORD size = sizeof(status_code);
		if (!WinHttpQueryHeaders(
			request,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&status_code,
			&size,
			WINHTTP_NO_HEADER_INDEX))
		{
			return 0;
		}
		return status_code;
	}

	std::string status_fallback_message(DWORD status)
	{
		switch (status)
		{
		case 400: return "Auth server rejected the request.";
		case 401: return "Invalid License Key!";
		case 403: return "License access was denied.";
		case 408: return "Auth server request timed out.";
		case 429: return "Too many authentication attempts. Please wait.";
		default:
			if (status >= 500)
				return "Auth server is temporarily unavailable.";
			return "Authentication Failed.";
		}
	}

	auth::service_result post_json_to_worker(
		const std::string& server_domain,
		const std::string& endpoint,
		std::string request_body)
	{
		auth::service_result result;
		if (server_domain.empty() || endpoint.empty() || endpoint.front() != '/')
		{
			result.message = "Config service is not configured.";
			return result;
		}

		winhttp_handle session(WinHttpOpen(
			L"Wolf-Client/2.1",
			WINHTTP_ACCESS_TYPE_NO_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0));
		if (!session)
		{
			result.message = "Failed to initialize config connection.";
			return result;
		}

		WinHttpSetTimeouts(session.get(), 5000, 5000, 10000, 15000);
		BOOL ipv6_fast_fallback = TRUE;
		WinHttpSetOption(session.get(), WINHTTP_OPTION_IPV6_FAST_FALLBACK,
			&ipv6_fast_fallback, sizeof(ipv6_fast_fallback));

		const std::wstring wide_domain = utf8_to_wide(server_domain);
		const std::wstring wide_endpoint = utf8_to_wide(endpoint);
		if (wide_domain.empty() || wide_endpoint.empty())
		{
			result.message = "Invalid config service address.";
			return result;
		}

		winhttp_handle connection(WinHttpConnect(
			session.get(), wide_domain.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
		if (!connection)
		{
			result.message = "Failed to connect to config service.";
			return result;
		}

		winhttp_handle request(WinHttpOpenRequest(
			connection.get(), L"POST", wide_endpoint.c_str(), nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
		if (!request)
		{
			result.message = "Failed to create config request.";
			return result;
		}

		constexpr wchar_t headers[] =
			L"Content-Type: application/json; charset=utf-8\r\n"
			L"Accept: application/json\r\n"
			L"Cache-Control: no-store\r\n";
		const BOOL sent = WinHttpSendRequest(
			request.get(), headers, static_cast<DWORD>(-1L),
			request_body.data(), static_cast<DWORD>(request_body.size()),
			static_cast<DWORD>(request_body.size()), 0);
		secure_clear(request_body);
		if (!sent)
		{
			result.message = "WinHTTP error " + std::to_string(GetLastError()) + ".";
			return result;
		}
		if (!WinHttpReceiveResponse(request.get(), nullptr))
		{
			result.message = "WinHTTP error " + std::to_string(GetLastError()) + ".";
			return result;
		}

		result.status_code = static_cast<int>(query_http_status(request.get()));
		if (!read_response_body(request.get(), result.body))
		{
			result.message = "Failed to read config service response.";
			return result;
		}
		result.success = result.status_code >= 200 && result.status_code < 300;
		if (!result.success)
		{
			result.message = parse_json_value(result.body, "message")
				.value_or(status_fallback_message(static_cast<DWORD>(result.status_code)));
		}
		return result;
	}

	auth_attempt authenticate_impl(
		const std::string& key_code,
		const std::string& server_domain,
		bool commit_session)
	{
		auth_attempt attempt;
		auth::auth_result& result = attempt.result;

		if (key_code.empty())
		{
			result.message = "License Key cannot be empty.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		if (commit_session)
		{
			result.hwid = auth::generate_hwid();
		}
		else
		{
			// A heartbeat must use the exact fingerprint accepted during login.
			// Recomputing it on a different worker thread previously allowed
			// core-local CPUID values to change the fingerprint mid-session.
			std::lock_guard<std::mutex> lock(g_hwid_mutex);
			result.hwid = g_session_hwid;
		}
		if (result.hwid.empty())
		{
			result.message = "Failed to generate a secure device fingerprint.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		const std::string nonce = auth::generate_nonce();
		if (nonce.empty())
		{
			result.message = "Failed to generate a secure nonce.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		const std::string nonce_hash = sha256_hex(nonce);
		if (nonce_hash.empty())
		{
			result.message = "Failed to hash the authentication nonce.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		winhttp_handle session(WinHttpOpen(
			L"Wolf-Client/2.1",
			WINHTTP_ACCESS_TYPE_NO_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0));
		if (!session)
		{
			result.message = "Failed to initialize WinHTTP session.";
			attempt.failure = failure_class::transient;
			return attempt;
		}

		if (!WinHttpSetTimeouts(session.get(), 5000, 5000, 10000, 15000))
		{
			result.message = "Failed to configure network timeouts.";
			attempt.failure = failure_class::transient;
			return attempt;
		}

		// Cloudflare publishes both IPv6 and IPv4 records. Some Windows setups
		// resolve IPv6 successfully even though the actual IPv6 route is unusable.
		// WinHTTP's fast fallback moves to IPv4 after a short failed IPv6 attempt.
		BOOL ipv6_fast_fallback = TRUE;
		WinHttpSetOption(
			session.get(),
			WINHTTP_OPTION_IPV6_FAST_FALLBACK,
			&ipv6_fast_fallback,
			sizeof(ipv6_fast_fallback));

		const std::wstring wide_domain = utf8_to_wide(server_domain);
		if (wide_domain.empty())
		{
			result.message = "Invalid auth server domain.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		winhttp_handle connection(WinHttpConnect(
			session.get(),
			wide_domain.c_str(),
			INTERNET_DEFAULT_HTTPS_PORT,
			0));
		if (!connection)
		{
			result.message = "Failed to connect to auth server.";
			attempt.failure = failure_class::transient;
			return attempt;
		}

		winhttp_handle request(WinHttpOpenRequest(
			connection.get(),
			L"POST",
			L"/api/v1/auth",
			nullptr,
			WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE));
		if (!request)
		{
			result.message = "Failed to create HTTP request.";
			attempt.failure = failure_class::transient;
			return attempt;
		}

		const std::string request_type =
			commit_session ? "activate" : "heartbeat";

		std::string request_body = commit_session
			? "{\"key\":\"" + json_escape(key_code) +
				"\",\"hwid\":\"" + json_escape(result.hwid) +
				"\",\"nonce\":\"" + json_escape(nonce) +
				"\",\"timestamp\":" + std::to_string(unix_time_ms()) +
				",\"request_type\":\"activate\"}"
			: "{\"session_token\":\"" + json_escape(key_code) +
				"\",\"hwid\":\"" + json_escape(result.hwid) +
				"\",\"nonce\":\"" + json_escape(nonce) +
				"\",\"timestamp\":" + std::to_string(unix_time_ms()) +
				",\"request_type\":\"heartbeat\"}";

		constexpr wchar_t headers[] =
			L"Content-Type: application/json; charset=utf-8\r\n"
			L"Accept: application/json\r\n"
			L"Cache-Control: no-store\r\n";

		BOOL success = WinHttpSendRequest(
			request.get(),
			headers,
			static_cast<DWORD>(-1L),
			const_cast<char*>(request_body.data()),
			static_cast<DWORD>(request_body.size()),
			static_cast<DWORD>(request_body.size()),
			0);
		DWORD network_error = success ? ERROR_SUCCESS : GetLastError();
		secure_clear(request_body);
		if (success)
		{
			success = WinHttpReceiveResponse(request.get(), nullptr);
			if (!success)
				network_error = GetLastError();
		}

		if (!success)
		{
			result.message = "WinHTTP error " + std::to_string(network_error) + ".";
			attempt.failure = failure_class::transient;
			return attempt;
		}

		const DWORD status_code = query_http_status(request.get());
		std::string response_text;
		if (!read_response_body(request.get(), response_text))
		{
			result.message = "Failed to read auth server response.";
			attempt.failure = failure_class::transient;
			return attempt;
		}

		const std::string server_message =
			parse_json_value(response_text, "message").value_or("");

		if (status_code < 200 || status_code >= 300)
		{
			result.message = server_message.empty()
				? status_fallback_message(status_code)
				: server_message;

			attempt.failure =
				(status_code == 408 || status_code == 429 || status_code >= 500 || status_code == 0)
				? failure_class::transient
				: failure_class::hard;
			return attempt;
		}

		const std::string encoded_payload =
			parse_json_value(response_text, "payload").value_or("");
		const std::string signature =
			parse_json_value(response_text, "signature").value_or("");
		std::vector<UCHAR> payload_bytes;
		if (encoded_payload.empty() || signature.empty() ||
			!base64url_decode(encoded_payload, payload_bytes))
		{
			result.message = "Auth response did not contain a valid signed payload.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		const std::string signed_payload(
			reinterpret_cast<const char*>(payload_bytes.data()), payload_bytes.size());
		SecureZeroMemory(payload_bytes.data(), payload_bytes.size());
		if (!verify_signed_payload(signed_payload, signature))
		{
			result.message = "Auth response signature verification failed.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		if (!parse_json_bool(signed_payload, "success", false))
		{
			result.message = "Authentication Failed.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		const std::string response_nonce_hash =
			parse_json_value(signed_payload, "nonce_hash").value_or("");
		const auto epoch = parse_integer<int>(
			parse_json_value(signed_payload, "license_epoch"));
		const auto expiration = parse_integer<long long>(
			parse_json_value(signed_payload, "exp"));

		if (signature.empty())
		{
			result.message = "Auth response did not contain a session token.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		if (!constant_time_equal(response_nonce_hash, nonce_hash))
		{
			result.message = "Auth response nonce validation failed.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		if (!epoch || *epoch <= 0)
		{
			result.message = "Auth response contained an invalid license epoch.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		if (!expiration || *expiration <= 0)
		{
			result.message = "Auth response did not contain a valid expiration time.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		constexpr long long allowed_clock_skew_ms = 30'000;
		if (*expiration + allowed_clock_skew_ms < unix_time_ms())
		{
			result.message = "Auth session token has already expired.";
			attempt.failure = failure_class::hard;
			return attempt;
		}

		result.success = true;
		result.message = "Authentication Successful!";
		result.license_epoch = *epoch;
		result.exp = *expiration;
		result.expires_at = parse_json_value(signed_payload, "expires_at").value_or("lifetime");
		result.session_token = encoded_payload + "." + signature;

		if (commit_session)
			auth::set_authenticated(true, result.session_token, result.license_epoch);

		attempt.failure = failure_class::none;
		return attempt;
	}
}

namespace auth
{
	std::string generate_hwid()
	{
		std::ostringstream raw;
		raw << "WOLF-HWID-V3|";

		int cpu_info[4]{};
		__cpuid(cpu_info, 0);
		raw << "cpu0="
			<< std::hex << cpu_info[0] << ':' << cpu_info[1] << ':'
			<< cpu_info[2] << ':' << cpu_info[3] << '|';

		__cpuid(cpu_info, 1);
		// CPUID.1:EBX contains the initial APIC id in bits 31:24. That id is
		// local to the logical processor executing this thread, so including
		// EBX made the HWID change between injections and between auth/heartbeat
		// threads. EAX/ECX/EDX describe the CPU model and feature set and are
		// stable across logical processors.
		raw << "cpu1="
			<< std::hex << cpu_info[0] << ':'
			<< cpu_info[2] << ':' << cpu_info[3] << '|';

		DWORD volume_serial = 0;
		wchar_t system_root[] = L"C:\\";
		wchar_t windows_directory[MAX_PATH]{};
		if (GetWindowsDirectoryW(windows_directory, MAX_PATH) >= 2 && windows_directory[1] == L':')
			system_root[0] = windows_directory[0];
		if (GetVolumeInformationW(
			system_root,
			nullptr,
			0,
			&volume_serial,
			nullptr,
			nullptr,
			nullptr,
			0))
		{
			raw << "volume=" << std::hex << volume_serial << '|';
		}

		const std::string machine_guid = read_machine_guid();
		if (!machine_guid.empty())
			raw << "machine=" << machine_guid << '|';

		// Fallback only. Computer name is deliberately not used when stronger
		// components are available because renaming the PC should not break HWID.
		if (machine_guid.empty() && volume_serial == 0)
		{
			wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1]{};
			DWORD length = MAX_COMPUTERNAME_LENGTH + 1;
			if (GetComputerNameW(computer_name, &length))
				raw << "computer=" << wide_to_utf8(computer_name) << '|';
		}

		return sha256_hex(raw.str());
	}

	std::string generate_nonce()
	{
		std::array<UCHAR, 16> random_bytes{};
		const NTSTATUS status = BCryptGenRandom(
			nullptr,
			random_bytes.data(),
			static_cast<ULONG>(random_bytes.size()),
			BCRYPT_USE_SYSTEM_PREFERRED_RNG);
		if (status < 0)
			return {};
		return bytes_to_hex(random_bytes.data(), random_bytes.size());
	}

	bool is_authenticated()
	{
		return g_authenticated.load(std::memory_order_acquire);
	}

	int get_active_epoch()
	{
		return g_active_epoch.load(std::memory_order_acquire);
	}

	void set_authenticated(bool status, const std::string& token, int epoch)
	{
		{
			std::lock_guard<std::mutex> lock(g_token_mutex);
			secure_clear(g_session_token);
			if (status)
				g_session_token = token;
		}

		if (status)
			g_active_epoch.store(epoch, std::memory_order_release);
		g_authenticated.store(status, std::memory_order_release);
	}

	void stop_heartbeat_monitor()
	{
		bool called_from_heartbeat = false;
		{
			std::lock_guard<std::mutex> lock(g_heartbeat_mutex);
			called_from_heartbeat =
				std::this_thread::get_id() == g_heartbeat_thread_id;
			g_heartbeat_running.store(false, std::memory_order_release);
		}
		g_heartbeat_cv.notify_all();

		if (called_from_heartbeat)
			return;

		std::unique_lock<std::mutex> lock(g_heartbeat_mutex);
		g_heartbeat_cv.wait(lock, [] {
			return g_heartbeat_exited.load(std::memory_order_acquire);
		});
	}

	void controlled_shutdown()
	{
		g_authenticated.store(false, std::memory_order_release);
		stop_heartbeat_monitor();

		std::lock_guard<std::mutex> lock(g_token_mutex);
		secure_clear(g_session_token);
		std::lock_guard<std::mutex> domain_lock(g_service_domain_mutex);
		secure_clear(g_service_domain);
		std::lock_guard<std::mutex> hwid_lock(g_hwid_mutex);
		secure_clear(g_session_hwid);
	}

	auth_result authenticate(
		const std::string& key_code,
		const std::string& server_domain)
	{
		auth_attempt attempt = authenticate_impl(key_code, server_domain, true);
		if (!attempt.result.success)
			controlled_shutdown();
		else
		{
			{
				std::lock_guard<std::mutex> hwid_lock(g_hwid_mutex);
				g_session_hwid = attempt.result.hwid;
			}
			std::lock_guard<std::mutex> lock(g_service_domain_mutex);
			g_service_domain = server_domain;
		}
		return std::move(attempt.result);
	}

	void configure_config_service(const std::string& server_domain)
	{
		const std::string device_id = generate_hwid();
		if (device_id.empty())
			return;

		{
			std::lock_guard<std::mutex> hwid_lock(g_hwid_mutex);
			g_session_hwid = device_id;
		}
		{
			std::lock_guard<std::mutex> domain_lock(g_service_domain_mutex);
			g_service_domain = server_domain;
		}
	}

	service_result post_authenticated_json(
		const std::string& path,
		const std::string& json_fields)
	{
		service_result result;
		std::string session_token;
		std::string server_domain;
		std::string device_id;
		{
			std::lock_guard<std::mutex> token_lock(g_token_mutex);
			session_token = g_session_token;
		}
		{
			std::lock_guard<std::mutex> domain_lock(g_service_domain_mutex);
			server_domain = g_service_domain;
		}
		{
			std::lock_guard<std::mutex> hwid_lock(g_hwid_mutex);
			device_id = g_session_hwid;
		}
		if (server_domain.empty() || device_id.empty())
		{
			secure_clear(session_token);
			result.message = "Config session is unavailable.";
			return result;
		}

		std::string body = "{\"device_id\":\"" + json_escape(device_id) + "\"";
		secure_clear(device_id);
		if (!session_token.empty())
			body += ",\"session_token\":\"" + json_escape(session_token) + "\"";
		secure_clear(session_token);
		if (!json_fields.empty())
			body += "," + json_fields;
		body += "}";
		return post_json_to_worker(server_domain, path, std::move(body));
	}

	void start_heartbeat_monitor(const std::string& server_url)
	{
		bool expected = false;
		if (!g_heartbeat_running.compare_exchange_strong(expected, true))
			return;

		{
			std::lock_guard<std::mutex> lock(g_heartbeat_mutex);
			g_heartbeat_exited.store(false, std::memory_order_release);
			g_heartbeat_thread_id = {};
		}

		try
		{
			std::thread([server_url]() {
				{
					std::lock_guard<std::mutex> lock(g_heartbeat_mutex);
					g_heartbeat_thread_id = std::this_thread::get_id();
				}

				std::array<UCHAR, 4> seed_bytes{};
				BCryptGenRandom(
					nullptr,
					seed_bytes.data(),
					static_cast<ULONG>(seed_bytes.size()),
					BCRYPT_USE_SYSTEM_PREFERRED_RNG);

				const std::uint32_t seed =
					(static_cast<std::uint32_t>(seed_bytes[0]) << 24) |
					(static_cast<std::uint32_t>(seed_bytes[1]) << 16) |
					(static_cast<std::uint32_t>(seed_bytes[2]) << 8) |
					static_cast<std::uint32_t>(seed_bytes[3]);

				std::mt19937 jitter_generator(seed);
				std::uniform_int_distribution<int> jitter_seconds(55, 75);
				int consecutive_transient_failures = 0;

				while (g_heartbeat_running.load(std::memory_order_acquire))
				{
					const auto regular_delay = std::chrono::seconds(jitter_seconds(jitter_generator));
					const auto retry_delay = consecutive_transient_failures == 0
						? regular_delay
						: (std::min)(
							kHeartbeatRetryInitial * (1 << (std::min)(consecutive_transient_failures - 1, 3)),
							kHeartbeatRetryMaximum);

					std::unique_lock<std::mutex> wait_lock(g_heartbeat_mutex);
					const bool stopping = g_heartbeat_cv.wait_for(
						wait_lock,
						retry_delay,
						[] {
							return !g_heartbeat_running.load(std::memory_order_acquire);
						});
					wait_lock.unlock();

					if (stopping || !g_authenticated.load(std::memory_order_acquire))
						break;

					const int expected_epoch =
						g_active_epoch.load(std::memory_order_acquire);
					std::string session_token;
					{
						std::lock_guard<std::mutex> token_lock(g_token_mutex);
						session_token = g_session_token;
					}
					auth_attempt heartbeat =
						authenticate_impl(session_token, server_url, false);
					secure_clear(session_token);

					if (heartbeat.result.success)
					{
						consecutive_transient_failures = 0;

						// Compare with the epoch captured before the request. The old code
						// updated g_active_epoch inside authenticate() first, making this
						// comparison meaningless and allowing epoch changes to slip through.
						if (heartbeat.result.license_epoch != expected_epoch)
						{
							controlled_shutdown();
							break;
						}

						set_authenticated(
							true,
							heartbeat.result.session_token,
							expected_epoch);
						continue;
					}

					if (heartbeat.failure == failure_class::hard)
					{
						controlled_shutdown();
						break;
					}

					// Transport failures are retried with a short capped backoff.  A
					// signed hard failure above remains the only condition that closes
					// an already authenticated client.
					++consecutive_transient_failures;
				}

				{
					std::lock_guard<std::mutex> lock(g_heartbeat_mutex);
					g_heartbeat_thread_id = {};
					g_heartbeat_running.store(false, std::memory_order_release);
					g_heartbeat_exited.store(true, std::memory_order_release);
				}
				g_heartbeat_cv.notify_all();
			}).detach();
		}
		catch (const std::system_error&)
		{
			{
				std::lock_guard<std::mutex> lock(g_heartbeat_mutex);
				g_heartbeat_running.store(false, std::memory_order_release);
				g_heartbeat_exited.store(true, std::memory_order_release);
				g_heartbeat_thread_id = {};
			}
			g_heartbeat_cv.notify_all();
			controlled_shutdown();
		}
	}
}
