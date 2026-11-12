// Fuzzes the sample importers (WAV/IFF/AIFF/SF2/raw) and re-exports the loaded sample.
#include "fuzz_common.h"
#include "XModule.h"
#include "SampleLoaderGeneric.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size > (4u << 20))
		return -1;
	FuzzTempDir tmp(data, size);
	if (!tmp.good())
		return -1;

	XModule module;
	module.createEmptySong();

	SampleLoaderGeneric loader(tmp.in(), module);
	if (!loader.identifySample())
		return 0;

	mp_sint32 channels = loader.getNumChannels();
	if (channels > 4)
		channels = 4;
	for (mp_sint32 c = 0; c < channels; c++)
		(void)loader.getChannelName(c);

	if (loader.loadSample(0, 0) == 0)
	{
		loader.saveSample(tmp.out(), 0, SampleLoaderGeneric::OutputFiletypeWAV);
		loader.saveSample(tmp.out(), 0, SampleLoaderGeneric::OutputFiletypeIFF);
		loader.saveSample(tmp.out(), 0, SampleLoaderGeneric::OutputFiletypeAIFF);
		loader.saveSample(tmp.out(), 0, SampleLoaderGeneric::OutputFiletypeRAW);
	}
	return 0;
}
