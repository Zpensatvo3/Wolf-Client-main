#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/anchor_macro_settings.hpp"
#include <memory>

namespace modules
{
class anchor_macro : public module
	{
	public:
		anchor_macro();
		~anchor_macro() override;

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		bool render_when_disabled() const override { return true; }
		float get_options_height() override { return 490.f; }
		void visit_config(config_visitor& v) override
		{
            settings_.visit(v,module_settings::anchor_macro::schema());
}

	private:
		struct runtime;
		std::unique_ptr<runtime> state_;
		settings::store<module_settings::anchor_macro> settings_;
		std::atomic_bool reset_requested_{true};
	};
}
