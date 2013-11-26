/*
 * NetClean VideoPlayer
 *  Multi process video player for windows.
 *
 * Copyright (c) 2010-2013 NetClean Technologies AB
 * All Rights Reserved.
 *
 * This file is part of NetClean VideoPlayer.
 *
 * NetClean VideoPlayer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * NetClean VideoPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NetClean VideoPlayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BITMAP_H
#define BITMAP_H

#define DEBUG

#include <cstdlib>
#include "color.h"
#include "log.h"

class Bitmap
{
	public:
	Bitmap(int bytesPerPixel = 4);
	Bitmap(int w, int h, bool freeData = true, int bytesPerPixel = 4);
	Bitmap(int w, int h, void* data, int bytesPerPixel = 4);

	~Bitmap();

	void allocate(int w, int h);

	void copy(Bitmap& bitmap);

	inline void getPixel(int x, int y, Color& pixel)
	{
		if(x >= 0 && y >= 0 && x < w && y < h){
			if(bytesPerPixel == 3){
				pixel.set(((Color3i*)pixels)[x + y * w]);
			}else{
				pixel = ((Color*)pixels)[x + y * w];
			}
		}
	}

	inline void setPixel(int x, int y, const Color& pixel)
	{
		if(x >= 0 && y >= 0 && x < w && y < h){
			if(pixel.a == 255){
				if(bytesPerPixel == 3){
					((Color3i*)pixels)[x + y * w].set(pixel);
				}else{
					((Color*)pixels)[x + y * w] = pixel;
				}
			}else if (pixel.a != 0){
				if(bytesPerPixel == 3){
					Color3i& cur = ((Color3i*)pixels)[x + y * w];
					cur.r = (unsigned char)((float)cur.r * (1.0f - (float)pixel.a) + (float)pixel.r * (float)pixel.a);
					cur.g = (unsigned char)((float)cur.r * (1.0f - (float)pixel.a) + (float)pixel.r * (float)pixel.a);
					cur.b = (unsigned char)((float)cur.r * (1.0f - (float)pixel.a) + (float)pixel.r * (float)pixel.a);
				}else{
					Color& cur = ((Color*)pixels)[x + y * w];
					cur.r = (unsigned char)((float)cur.r * (1.0f - (float)pixel.a) + (float)pixel.r * (float)pixel.a);
					cur.g = (unsigned char)((float)cur.r * (1.0f - (float)pixel.a) + (float)pixel.r * (float)pixel.a);
					cur.b = (unsigned char)((float)cur.r * (1.0f - (float)pixel.a) + (float)pixel.r * (float)pixel.a);
					cur.a = (unsigned char)((float)cur.a / 2 + pixel.a / 2);
				}
			}
		}
	}

	inline void setPixel(int x, int y, int component, byte val)
	{
		if(x >= 0 && y >= 0 && x < w && y < h){
			((byte*)pixels)[(x + y * w) * bytesPerPixel + component] = val;
		}
	}

	inline void draw(Bitmap& target, int x = 0, int y = 0)
	{
		for(int iy = 0; iy < h; iy++){
			for(int ix = 0; ix < w; ix++){
				Color pixel;
				getPixel(ix, iy, pixel);
				target.setPixel(x + ix, y + iy, pixel);
			}
		}
	}

	inline void freePixels(){
		if(pixels) free(pixels);
	}

	void save(std::string filename);

	float getDistance(Bitmap& bitmap);

	void clear(byte r = 0, byte g = 0, byte b = 0, byte a = 0);
	void clear(Color& color);
	void randomize();

	int w, h;
	bool freeData;
	int bytesPerPixel;

	void* pixels;
	int allocated;
};

#endif
