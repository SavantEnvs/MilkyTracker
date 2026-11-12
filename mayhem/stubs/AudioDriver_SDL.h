// Stand-in for the SDL2 audio backend so upstream's AudioDriverManager.cpp builds without SDL.
#ifndef MAYHEM_AUDIODRIVER_SDL_STUB_H
#define MAYHEM_AUDIODRIVER_SDL_STUB_H

#include "AudioDriver_NULL.h"

class AudioDriver_SDL : public AudioDriver_NULL
{
};

#endif
