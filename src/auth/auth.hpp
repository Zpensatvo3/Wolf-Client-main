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

	// Launcher-only login. Production DLL builds cannot activate a license.
	auth_result authenticate(const std::string& key_code, const std::string& server_url);

	// Retained initialization hook; never creates a config-only permission.
	void configure_config_service(const std::string& server_url);

	// Renews the process/build-bound runtime lease every 18-22 seconds.
	// First network/HTTP/verification failure revokes it without retry/grace.
	// The long-lived license key is never retained.
	void start_heartbeat_monitor(const std::string& server_url);

	// Stops and waits for the heartbeat thread. Call before unloading the DLL.
	void stop_heartbeat_monitor();

	// Revokes local authentication state and securely clears the stored token.
	void controlled_shutdown();


    enum class scope : unsigned { modules=1, input=2, bridge=4, configs=8, render=16, launcher=32 };
    // UI state is not an authority. Each consumer requests its own capability.
    bool permitted(scope capability);
    bool is_authenticated();
	int get_active_epoch();

	// Posts JSON fields to the authenticated Worker configured during login.
	// Example fields: \"id\": 12. The function adds the session token safely.
	service_result post_authenticated_json(const std::string& path, const std::string& json_fields);

    // Launcher protocol. No permanent license key crosses the local handoff.
    std::string make_launcher_request(const std::string& path, const std::string& json_fields);
    std::string service_field(const std::string& json, const std::string& key);
    std::string decode_service_payload(const std::string& envelope, const std::string& audience);
    auth_result redeem_launcher_ticket(const std::string& handoff, const std::string& server_domain,
        const std::string& release_sha256, const std::string& minecraft_target);
}

#endif // ONESTEP_AUTH_HPP
