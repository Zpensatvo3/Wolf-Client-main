#include "auth.hpp"
#include "config.hpp"
#include "session_policy.hpp"
#include "device_proof.hpp"
#include "launcher_protocol.hpp"

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
#include <memory>
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

namespace
{
	constexpr std::size_t kMaxResponseBytes = 1024 * 1024;
	constexpr char kAuthPublicKeySpkiBase64[] = ONESTEP_AUTH_PUBLIC_KEY_SPKI_BASE64;

	std::atomic<bool> g_heartbeat_running{ false };
	std::mutex g_heartbeat_owner_mutex;
	std::thread g_heartbeat_thread;
	std::mutex g_heartbeat_mutex;
	std::condition_variable g_heartbeat_cv;
	std::thread::id g_heartbeat_thread_id{};

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

		HINTERNET get() const noexcept { return value_.load(); }
		explicit operator bool() const noexcept { return value_.load() != nullptr; }

		HINTERNET release() noexcept
		{
			return value_.exchange(nullptr);
		}

		void reset(HINTERNET replacement = nullptr) noexcept
		{
			if (auto old = value_.exchange(replacement)) WinHttpCloseHandle(old);
		}

	private:
		std::atomic<HINTERNET> value_{nullptr};
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

	__declspec(noinline) bool verify_signed_payload(
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
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		auth::service_result result;
		if (!auth::policy::valid_domain(server_domain) || endpoint.empty() || endpoint.front() != '/')
		{
			result.message = "Config service is not configured.";
			return result;
		}

		winhttp_handle session(WinHttpOpen(
			L"OneStep-Client/2.1",
			WINHTTP_ACCESS_TYPE_NO_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0));
		if (!session)
		{
			result.message = "Failed to initialize config connection.";
			return result;
		}

		WinHttpSetTimeouts(session.get(), 5000, 5000, 5000, 5000);
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
		DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
		if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY,
			&redirect_policy, sizeof(redirect_policy)))
		{
			result.message = "Failed to secure config connection.";
			return result;
		}

        struct timeout_guard {
            winhttp_handle& request; std::mutex mutex; std::condition_variable cv; bool done=false; std::thread timer;
            timeout_guard(winhttp_handle& h,std::chrono::steady_clock::time_point limit):request(h),
                timer([this,limit]{std::unique_lock lock(mutex); if(!cv.wait_until(lock,limit,[this]{return done;})) request.reset();}){}
            ~timeout_guard(){ {std::lock_guard lock(mutex);done=true;} cv.notify_all();timer.join(); }
        } timeout(request,deadline);

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
		if (std::chrono::steady_clock::now() >= deadline) {
            result.success=false;result.message="Request timed out.";secure_clear(result.body);
        }
		return result;
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

}

#include "protocol_v2.inl"
