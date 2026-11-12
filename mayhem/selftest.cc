// Known-answer probe for the MilkyPlay loaders and mixer. Prints `key=value` lines; mayhem/test.sh
// compares them against exact expected values.
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <vector>
#include "render_common.h"
#include "SampleLoaderGeneric.h"
#include "XIInstrument.h"
#include "Decompressor.h"
#include "XMFile.h"

static uint64_t fnv(const void* p, size_t n, uint64_t h = 1469598103934665603ULL)
{
	const unsigned char* b = (const unsigned char*)p;
	for (size_t i = 0; i < n; i++)
		h = (h ^ b[i]) * 1099511628211ULL;
	return h;
}

static bool writeFile(const char* path, const std::vector<unsigned char>& v)
{
	FILE* f = fopen(path, "wb");
	if (!f)
		return false;
	bool ok = fwrite(v.data(), 1, v.size(), f) == v.size();
	fclose(f);
	return ok;
}

static void put16be(std::vector<unsigned char>& v, unsigned x) { v.push_back(x >> 8); v.push_back(x & 255); }
static void put16le(std::vector<unsigned char>& v, unsigned x) { v.push_back(x & 255); v.push_back(x >> 8); }
static void put32le(std::vector<unsigned char>& v, unsigned x) { put16le(v, x & 0xffff); put16le(v, x >> 16); }

static void reportModule(const char* tag, XModule& m)
{
	uint64_t h = 1469598103934665603ULL;
	unsigned long bytes = 0;
	for (mp_uint32 i = 0; i < (mp_uint32)m.header.smpnum; i++)
	{
		if (!m.smp[i].sample)
			continue;
		size_t n = m.smp[i].samplen * ((m.smp[i].type & 16) ? 2 : 1);
		h = fnv(m.smp[i].sample, n, h);
		bytes += n;
	}
	printf("%s name=[%s] channels=%d orders=%d patterns=%d instruments=%d samples=%d tempo=%d speed=%d\n",
		   tag, m.header.name, (int)m.header.channum, (int)m.header.ordnum, (int)m.header.patnum,
		   (int)m.header.insnum, (int)m.header.smpnum, (int)m.header.tempo, (int)m.header.speed);
	printf("%s sampledata bytes=%lu fnv=%016llx\n", tag, bytes, (unsigned long long)h);
}

int main(int argc, char** argv)
{
	char dirTemplate[] = "/tmp/mmselftestXXXXXX";
	if (!mkdtemp(dirTemplate))
		return 2;
	std::string dir(dirTemplate);
	int rc = 0;

	// 1) Files named on the command line: shipped demo tunes (XM), seed modules and an XI instrument.
	for (int i = 1; i < argc; i++)
	{
		const char* base = strrchr(argv[i], '/');
		base = base ? base + 1 : argv[i];
		size_t len = strlen(base);
		if (len > 3 && !strcmp(base + len - 3, ".xi"))
		{
			XIInstrument ins;
			mp_sint32 res = ins.load(argv[i]);
			printf("xi %s load=%d\n", base, (int)res);
			if (res == MP_OK)
			{
				printf("xi %s name=[%s] samples=%d len=%d vol=%d volfade=%d\n", base, (const char*)ins.name,
					   (int)ins.numsamples, (int)ins.samples[0].samplen, (int)ins.samples[0].vol, (int)ins.volfade);
				printf("xi %s data=", base);
				for (int k = 0; k < 16; k++)
					printf("%d%s", (int)ins.samples[0].sample[k], k < 15 ? "," : "\n");
			}
			continue;
		}
		XModule m;
		mp_sint32 res = m.loadModule(argv[i]);
		printf("module %s load=%d\n", base, (int)res);
		if (res != MP_OK)
			continue;
		reportModule((std::string("module ") + base).c_str(), m);
		ChecksumDriver d;
		mp_sint32 frames = renderModule(m, 0, d);
		printf("module %s render frames=%d fnv=%016llx\n", base, (int)frames, (unsigned long long)d.sum);
	}

	// 2) A hand-built ProTracker module: 4 channels, 1 pattern, 1 looped 32-byte sample.
	{
		std::vector<unsigned char> mod;
		char title[20] = "mayhem kat";
		mod.insert(mod.end(), title, title + 20);
		for (int s = 0; s < 31; s++)
		{
			char nm[22] = {0};
			if (s == 0)
				strcpy(nm, "ramp");
			mod.insert(mod.end(), nm, nm + 22);
			put16be(mod, s == 0 ? 16 : 0);
			mod.push_back(0);
			mod.push_back(s == 0 ? 64 : 0);
			put16be(mod, 0);
			put16be(mod, s == 0 ? 16 : 1);
		}
		mod.push_back(1);
		mod.push_back(127);
		for (int o = 0; o < 128; o++)
			mod.push_back(0);
		mod.push_back('M'); mod.push_back('.'); mod.push_back('K'); mod.push_back('.');
		for (int row = 0; row < 64; row++)
			for (int ch = 0; ch < 4; ch++)
			{
				if (row == 0 && ch == 0)
				{
					mod.push_back(0x01); mod.push_back(0xAC);	// period 0x1AC = 428 (C-2)
					mod.push_back(0x10); mod.push_back(0x00);	// sample 1, no effect
				}
				else
					for (int k = 0; k < 4; k++) mod.push_back(0);
			}
		for (int k = 0; k < 32; k++)
			mod.push_back((unsigned char)(k * 8 - 128));
		std::string p = dir + "/kat.mod";
		if (!writeFile(p.c_str(), mod))
			return 2;
		XModule m;
		mp_sint32 res = m.loadModule(p.c_str());
		printf("mod load=%d\n", (int)res);
		if (res == MP_OK)
		{
			reportModule("mod", m);
			printf("mod cell0 note=%d ins=%d\n", (int)m.phead[0].patternData[0], (int)m.phead[0].patternData[1]);
			printf("mod sample0 len=%d vol=%d\n", (int)m.smp[0].samplen, (int)m.smp[0].vol);
			ChecksumDriver d;
			mp_sint32 frames = renderModule(m, 0, d);
			printf("mod render frames=%d fnv=%016llx\n", (int)frames, (unsigned long long)d.sum);
		}
	}

	// 3) A hand-built 8-bit mono WAV through the sample importer, then re-exported as WAV.
	{
		std::vector<unsigned char> wav;
		const char* riff = "RIFF"; wav.insert(wav.end(), riff, riff + 4);
		put32le(wav, 36 + 8);
		const char* wavefmt = "WAVEfmt "; wav.insert(wav.end(), wavefmt, wavefmt + 8);
		put32le(wav, 16); put16le(wav, 1); put16le(wav, 1);
		put32le(wav, 8363); put32le(wav, 8363); put16le(wav, 1); put16le(wav, 8);
		const char* dat = "data"; wav.insert(wav.end(), dat, dat + 4);
		put32le(wav, 8);
		const unsigned char pcm[8] = {0x80, 0xA0, 0xC0, 0xE0, 0x00, 0x20, 0x40, 0x60};
		wav.insert(wav.end(), pcm, pcm + 8);
		std::string p = dir + "/kat.wav";
		if (!writeFile(p.c_str(), wav))
			return 2;
		XModule m;
		m.createEmptySong();
		SampleLoaderGeneric ld(p.c_str(), m);
		bool id = ld.identifySample();
		printf("wav identify=%d channels=%d\n", (int)id, (int)ld.getNumChannels());
		mp_sint32 res = ld.loadSample(0, 0);
		printf("wav load=%d len=%d type=%d\n", (int)res, (int)m.smp[0].samplen, (int)m.smp[0].type);
		if (res == 0 && m.smp[0].sample)
		{
			printf("wav data=");
			for (int i = 0; i < 8; i++)
				printf("%d%s", (int)((mp_sbyte*)m.smp[0].sample)[i], i < 7 ? "," : "\n");
			std::string out = dir + "/out.wav";
			mp_sint32 sres = ld.saveSample(out.c_str(), 0, SampleLoaderGeneric::OutputFiletypeWAV);
			FILE* f = fopen(out.c_str(), "rb");
			long sz = 0;
			if (f) { fseek(f, 0, SEEK_END); sz = ftell(f); fclose(f); }
			printf("wav save=%d size=%ld\n", (int)sres, sz);
		}
	}

	// 4) PowerPacker/LZX/... front-end must refuse plain data and accept a hand-made gzip stream.
	{
		std::vector<unsigned char> junk(64, 0x41);
		std::string p = dir + "/junk.bin";
		writeFile(p.c_str(), junk);
		Decompressor d((PPSystemString(p.c_str())));
		XMFile jf(p.c_str());
		printf("decompress junk identify=%d\n", (int)d.identify(jf));
		// gzip (FNAME flag set, as the identifier expects) holding the stored deflate block "milky\n"
		static const unsigned char gz[] = {
			0x1f,0x8b,0x08,0x08,0,0,0,0,0x00,0x03, 'k',0,
			0x01,0x06,0x00,0xf9,0xff,'m','i','l','k','y','\n',
			0xc0,0xef,0xd0,0xba, 0x06,0x00,0x00,0x00 };
		std::vector<unsigned char> g(gz, gz + sizeof(gz));
		std::string gp = dir + "/kat.gz";
		writeFile(gp.c_str(), g);
		Decompressor dg((PPSystemString(gp.c_str())));
		XMFile gf(gp.c_str());
		bool gid = dg.identify(gf);
		std::string go = dir + "/kat.out";
		bool gres = gid && dg.decompress(PPSystemString(go.c_str()), DecompressorBase::HintAll);
		char outBuf[16] = {0};
		FILE* of = fopen(go.c_str(), "rb");
		size_t got = of ? fread(outBuf, 1, sizeof(outBuf) - 1, of) : 0;
		if (of) fclose(of);
		for (size_t i = 0; i < got; i++) if (outBuf[i] == '\n') outBuf[i] = '|';
		printf("decompress gzip identify=%d result=%d out=[%s]\n", (int)gid, (int)gres, outBuf);
	}

	std::string cmd = "rm -rf '" + dir + "'";
	if (system(cmd.c_str()) != 0)
		rc = 2;
	return rc;
}
