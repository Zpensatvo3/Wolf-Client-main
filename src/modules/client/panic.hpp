#pragma once

#include "../module_support.hpp"

namespace modules
{
class panic : public module
	{
	public:
		panic() : module("panic", "Client") {}
		void run(::cache& cache) override;
	};
}
