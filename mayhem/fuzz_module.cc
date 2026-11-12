// Fuzzes the MilkyPlay module loaders (MOD/XM/S3M/IT/669/AMF/AMS/DBM/... auto-detected by content).
#include "fuzz_common.h"
#include "XModule.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size > (4u << 20))
		return -1;
	FuzzTempDir tmp(data, size);
	if (!tmp.good())
		return -1;

	XModule module;
	mp_sint32 res = module.loadModule(tmp.in());
	(void)res;
	return 0;
}
