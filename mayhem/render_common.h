#ifndef MAYHEM_RENDER_COMMON_H
#define MAYHEM_RENDER_COMMON_H

#include <stdint.h>
#include "AudioDriver_NULL.h"
#include "PlayerGeneric.h"
#include "XModule.h"

// Discards the mixed audio but keeps a running checksum, so the render path is observable.
class ChecksumDriver : public AudioDriver_NULL
{
public:
	uint64_t sum;
	uint64_t frames;
	ChecksumDriver() : sum(1469598103934665603ULL), frames(0) {}

	virtual const char* getDriverID() { return "Checksum"; }

	virtual void advance()
	{
		AudioDriver_NULL::advance();
		for (mp_sint32 i = 0; i < bufferSize; i++)
			sum = (sum ^ (uint16_t)compensateBuffer[i]) * 1099511628211ULL;
		frames += bufferSize / 2;
	}
};

// Renders order range [0, lastOrder] of a loaded module at a low sample rate; returns frames rendered.
static inline mp_sint32 renderModule(XModule& module, mp_sint32 lastOrder, ChecksumDriver& driver)
{
	PlayerGeneric player(8000);
	player.setBufferSize(1024);
	return player.exportToWAV(NULL, &module, 0, lastOrder, NULL, 0, NULL, &driver);
}

#endif
