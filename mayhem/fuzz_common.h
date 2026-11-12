#ifndef MAYHEM_FUZZ_COMMON_H
#define MAYHEM_FUZZ_COMMON_H

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Per-input scratch directory holding the fuzz input as a file (the milkyplay loaders are file based).
class FuzzTempDir
{
public:
	FuzzTempDir(const uint8_t* data, size_t size, const char* name = "input.bin") : ok(false)
	{
		const char* base = getenv("TMPDIR");
		if (!base || !*base)
			base = "/tmp";
		snprintf(dir, sizeof(dir), "%s/mmfuzzXXXXXX", base);
		if (!mkdtemp(dir))
			return;
		snprintf(inPath, sizeof(inPath), "%s/%s", dir, name);
		snprintf(outPath, sizeof(outPath), "%s/out.bin", dir);
		FILE* f = fopen(inPath, "wb");
		if (!f)
			return;
		ok = size == 0 || fwrite(data, 1, size, f) == size;
		fclose(f);
	}

	~FuzzTempDir()
	{
		DIR* d = opendir(dir);
		if (d)
		{
			struct dirent* e;
			while ((e = readdir(d)) != NULL)
			{
				if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
					continue;
				char p[1200];
				snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
				unlink(p);
			}
			closedir(d);
		}
		rmdir(dir);
	}

	bool good() const { return ok; }
	const char* in() const { return inPath; }
	const char* out() const { return outPath; }

private:
	char dir[1024] = "";
	char inPath[1100] = "";
	char outPath[1100] = "";
	bool ok;
};

#endif
