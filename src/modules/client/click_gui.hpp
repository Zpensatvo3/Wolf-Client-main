#pragma once

#include "../module_support.hpp"

namespace modules
{
class click_gui : public module
	{
	public:
		click_gui() : module("click gui", "Client") {}
		void render_options() override;
		float get_options_height() override { return 55.0f; }
	};
}
