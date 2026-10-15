#pragma once

#include "../module_support.hpp"

namespace modules
{
class sprint : public module
	{
	public:
		sprint() : module("sprint", "Movement") {}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 115.0f; }
		void visit_config(config_visitor& v) override { v.field("stop_while_using_item", stop_while_using_item); }

	private:
		bool stop_while_using_item = true;
		bool started_by_module = false;
	};
}
