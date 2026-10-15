#pragma once

#include "../module_support.hpp"

namespace modules
{
class full_bright : public module
	{
	public:
		full_bright() : module("full bright", "Visual") {}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;

	private:
		double original_gamma = 0.5;
		bool original_gamma_captured = false;
	};
}
