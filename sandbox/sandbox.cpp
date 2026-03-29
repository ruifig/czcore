import czcore;

#include "crazygaze/core/Logging_Macros.h"

int main()
{
	using namespace cz;
	cz::LogOutputs logs;

	auto ptr = makeShared<int>(5);
	auto sss = next_pow2(5);

	CZ_LOG(Main, Warning, "Hello World!");

	return EXIT_SUCCESS;
}

