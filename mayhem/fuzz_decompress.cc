// Fuzzes the archive/packer front-ends: PowerPacker, LZX, UMX, LHA, GZIP and ZIP.
#include "fuzz_common.h"
#include "Decompressor.h"
#include "XMFile.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size > (4u << 20))
		return -1;
	FuzzTempDir tmp(data, size);
	if (!tmp.good())
		return -1;

	Decompressor decompressor(PPSystemString(tmp.in()));
	XMFile in(tmp.in());
	if (decompressor.identify(in))
		decompressor.decompress(PPSystemString(tmp.out()), DecompressorBase::HintAll);
	return 0;
}
