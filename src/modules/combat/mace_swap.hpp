#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/mace_swap_settings.hpp"
#include "../../services/game/client_context.hpp"

namespace modules
{
class mace_swap : public module
	{
	public:
		mace_swap() : module("mace swap", "Combat") { active_instance.store(this); }
		~mace_swap() override
		{
			mace_swap* expected = this;
			active_instance.compare_exchange_strong(expected, nullptr);
		}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 200.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::mace_swap::schema()); }

		// Called on Minecraft's window/render thread before an attack reaches the
		// game. Both physical and tagged synthetic attacks share this coordinator.
		static void gate_left_click();
		static bool prepare_for_synthetic_attack();
		static bool prepare_for_trigger_attack();
		static int trigger_prepared_slot();
		static int starting_slot_if_owned(const maps::ClientPlayerEntity& player,
			const maps::ClientWorld& world, int selected);
		static void finish_trigger_preparation(const maps::ClientPlayerEntity& player,
			const maps::ClientPlayerInteractionManager& manager, bool consumed);

	private:
        modules::settings::store<module_settings::mace_swap> settings_;
		inline static std::atomic<mace_swap*> active_instance{ nullptr };
		int old_slot = -1;
		int mace_slot = -1;
		bool waiting_for_release = false;
		bool trigger_prepared = false;
		jni::object_wrapper trigger_world{nullptr, true}, trigger_player{nullptr, true};
		std::chrono::steady_clock::time_point switched_at{};
		std::mutex state_mutex{};

		bool prepare_current_thread();
		void restore(::cache& cache);
	};
}
