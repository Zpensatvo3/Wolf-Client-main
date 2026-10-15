#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/auto_tool_settings.hpp"

namespace modules
{
class auto_tool : public module
	{
	public:
		auto_tool() : module("auto tool", "World") {}

		void run(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 120.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::auto_tool::schema()); }

	private:
		modules::settings::store<module_settings::auto_tool> settings_;
		int last_selected_slot = -1;
	};
}
