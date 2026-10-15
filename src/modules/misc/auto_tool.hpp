#pragma once

#include "../module_support.hpp"

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
		{
			v.field("require_left_click", require_left_click); v.field("prefer_hotbar_only", prefer_hotbar_only);
		}

	private:
		bool require_left_click = true;
		bool prefer_hotbar_only = true;
		int last_selected_slot = -1;
	};
}
