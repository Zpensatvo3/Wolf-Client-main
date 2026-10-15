#pragma once

#include "../module_support.hpp"
#include "jump_reset_config.hpp"
#include "jump_reset_engine.hpp"
#include "../../mappings.hpp"

namespace modules
{
class jump_reset : public module
	{
	public:
		jump_reset() : module("jump reset", "Combat") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		bool render_when_disabled() const override { return true; }
		void on_enable(::cache& cache) override;
		void cancel_on_client_thread();
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 320.0f; }
		void visit_config(config_visitor& v) override
		{
			auto candidate = settings_snapshot();
			if (jump_reset_config::visit_config(v, candidate)) apply_settings(candidate, true);
		}
		static bool synthetic_space_active()
		{
			return synthetic_space_down.load(std::memory_order_acquire);
		}

	private:
		jump_reset_config::config settings{};
		std::mutex settings_mutex;
		std::atomic<std::uint64_t> settings_revision{1};
		std::uint64_t session = 0, seen_reset = 0;
		std::atomic<std::uint64_t> reset_generation{1};
		jump_reset_config::config settings_snapshot();
		void apply_settings(jump_reset_config::config candidate, bool force_reset = false);
		jump_reset_engine::runtime engine;
		jni::object_wrapper world{nullptr, true}, player{nullptr, true}, previous_damage_source{nullptr, true};
		std::uint64_t source_revision = 0, consumed_source_revision = 0;
		std::int64_t source_observed_at = 0;
		bool source_is_living = false;
		std::int64_t last_damage_at = -1;
		bool awaiting_damage_source = false;
		maps::KeyBinding owned_jump_key{nullptr, true};
		maps::InputKey owned_bound_key{nullptr, true};
		unsigned owned_virtual_key = 0, owned_scan_code = 0;
		std::atomic<std::uint32_t> space_ticket{0};
		std::atomic<int> last_issue{0};
		bool release_input();
		void request_reset();
		bool validate_space_delivery(std::uint64_t generation, std::uint64_t revision,
			std::uint64_t expected_session, std::uint64_t damage_id, std::int64_t expires_at);
		inline static std::atomic_bool synthetic_space_down{ false };
	};
}
