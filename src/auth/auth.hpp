#ifndef ONESTEP_AUTH_HPP
#define ONESTEP_AUTH_HPP

#include <atomic>
#include <string>

namespace auth
{
	struct auth_result
	{
		bool success = false;
		std::string message;
		std::string hwid;
		std::string expires_at;
		std::string session_token;
		int license_epoch = 1;
		long long exp = 0;
	};

	// Result for a small authenticated service request. The session token stays
	// inside the auth layer; callers only supply JSON fields after that token.
	struct service_result
	{
		bool success = false;
		int status_code = 0;
		std::string body;
		std::string message;
	};

	// Generates a SHA-256 device fingerprint from stable Windows/CPU components.
	// Existing licenses using the old 16-character HWID must be reset once.
	std::string generate_hwid();

	// Generates a 128-bit nonce with Windows CNG (BCryptGenRandom).
	std::string generate_nonce();

	// Performs the HTTPS authentication request against the Cloudflare Worker.
	auth_result authenticate(const std::string& key_code, const std::string& server_url);

	// Configs can run independently while licence enforcement is temporarily
	// disabled. This registers a stable device identity only for public-config
	// ownership; it does not authenticate or start a licence heartbeat.
	void configure_config_service(const std::string& server_url);

	// Starts the background heartbeat with randomized 55-75 second intervals.
	// Temporary network failures retry with bounded backoff until the signed
	// session expires. A monotonic deadline bounds offline access.
	// The long-lived license key is never retained for this operation; the
	// server-issued, signed session token is refreshed instead.
	void start_heartbeat_monitor(const std::string& server_url);

	// Stops and waits for the heartbeat thread. Call before unloading the DLL.
	void stop_heartbeat_monitor();

	// Revokes local authentication state and securely clears the stored token.
	void controlled_shutdown();

	bool is_authenticated();
	int get_active_epoch();
	void set_authenticated(bool status, const std::string& token = "", int epoch = 1,
		long long expires_at_ms = 0);

	// Posts JSON fields to the authenticated Worker configured during login.
	// Example fields: \"id\": 12. The function adds the session token safely.
	service_result post_authenticated_json(const std::string& path, const std::string& json_fields);

    // Launcher protocol. No permanent license key crosses the local handoff.
    std::string make_launcher_request(const std::string& json_fields);
    std::string service_field(const std::string& json, const std::string& key);
    std::string decode_service_payload(const std::string& envelope, const std::string& audience);
    auth_result redeem_launcher_ticket(const std::string& ticket, const std::string& server_domain,
        const std::string& release_sha256);
}

#endif // ONESTEP_AUTH_HPP
