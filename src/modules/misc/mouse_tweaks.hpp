#pragma once

#include "../module_support.hpp"

namespace modules
{
class mouse_tweaks : public module
	{
	public:
		mouse_tweaks() : module("mouse tweaks", "Player") {}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 120.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("right_click_tweak", right_click_tweak); v.field("left_click_with_item", left_click_with_item);
			v.field("left_click_without_item", left_click_without_item);
		}

	private:
		bool right_click_tweak = true;
		bool left_click_with_item = true;
		bool left_click_without_item = true;
	};
}
