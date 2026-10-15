#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/block_esp_settings.hpp"

namespace modules
{
class block_esp : public module
	{
	public:
		block_esp() : module("block esp", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 500.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::block_esp::schema()); }

	private:
        modules::settings::store<module_settings::block_esp> settings_;
		struct marker { double x, y, z; std::string key; };
		int scan_index = 0;
		int origin_x = 0, origin_y = 0, origin_z = 0;
		std::vector<marker> markers{};
		std::mutex marker_mutex{};
		std::vector<std::string> available_blocks{};
		bool registry_loaded = false;
		char block_search[64]{};
	};
}
