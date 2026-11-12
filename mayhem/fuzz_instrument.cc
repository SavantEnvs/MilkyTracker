// Fuzzes the XI (Extended Instrument) and GUS PAT instrument loaders.
#include "fuzz_common.h"
#include "XIInstrument.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size > (4u << 20))
		return -1;
	FuzzTempDir tmp(data, size);
	if (!tmp.good())
		return -1;

	XIInstrument ins;
	if (ins.load(tmp.in()) == 0)
	{
		XIInstrument copy(ins);
		(void)copy;
	}
	return 0;
}
