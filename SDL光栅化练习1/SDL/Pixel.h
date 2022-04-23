#pragma once
#include "SDL.h"
class Pixel
{
public: 
	Pixel();
	void putpixel(SDL_Surface* surface, int x, int y, Uint32 pixel);

};

