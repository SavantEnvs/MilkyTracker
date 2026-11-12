// Loads a module and renders its first order through the mixer and the player engines.
#include "fuzz_common.h"
#include "render_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size > (1u << 20))
		return -1;
	FuzzTempDir tmp(data, size);
	if (!tmp.good())
		return -1;

	XModule module;
	if (module.loadModule(tmp.in()) != MP_OK)
		return 0;

	ChecksumDriver driver;
	renderModule(module, 0, driver);
	return 0;
}
