#pragma once

#include "../module_support.hpp"

namespace modules
{
class fast_place : public module
	{
	public:
		fast_place() : module("fast place", "World") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 155.0f; }
		void visit_config(config_visitor& v) override { v.field("delay", delay); }

	private:
		int delay = 0;
	};
}
