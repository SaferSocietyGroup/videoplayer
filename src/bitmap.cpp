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

#include "bitmap.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <fstream>

Bitmap::Bitmap(int bytesPerPixel)
{
	this->bytesPerPixel = bytesPerPixel;
	pixels = 0;
	allocated = 0;
	w = h = 0;
	freeData = true;
}

Bitmap::Bitmap(int w, int h, bool freeData, int bytesPerPixel)
{
	this->bytesPerPixel = bytesPerPixel;
	pixels = 0;
	allocated = 0;
	allocate(w, h);
	this->freeData = freeData;
}

Bitmap::Bitmap(int w, int h, void* data, int bytesPerPixel)
{
	this->bytesPerPixel = bytesPerPixel;
	pixels = data;
	allocated = w * h;
	this->w = w;
	this->h = h;
	freeData = false;
}

void Bitmap::save(std::string filename)
{
	if(bytesPerPixel != 4) return;

	std::ofstream file(filename.c_str(), std::ios::binary);

	// TGA format
	
	file.put(0);					// 1	1 byte	ID length	Length of the image ID field
	file.put(0);					// 2	1 byte	Color map type	Whether a color map is included
	file.put(2);					// 3	1 byte	Image type	Compression and color types, 2 = true color uncompressed
	file.write("\0\0\0\0\0", 5);	// 4	5 bytes	Color map specification	Describes the color map

	// 5	10 bytes	Image specification	Image dimensions and format

	file.write("\0\0", 2);						// X-origin (2 bytes): absolute coordinate of lower-left corner for displays where origin is at the lower left
	file.write("\0\0", 2);						// Y-origin (2 bytes): as for X-origin
	file.put((unsigned char)(w & 0xff));		// Image width (2 bytes): width in pixels
	file.put((unsigned char)((w >> 8) & 0xff)); 
	file.put((unsigned char)(h & 0xff));		// Image height (2 bytes): height in pixels
	file.put((unsigned char)((h >> 8) & 0xff)); 
	file.put(32);								// Pixel depth (1 byte): bits per pixel
	file.put(0);								// Image descriptor (1 byte): bits 3-0 give the alpha channel depth, bits 5-4 give direction

	for(int y = h - 1; y >= 0; y--){
		for(int x = 0; x < w; x++){
			int i = x + y * w;
			file.put(((Color*)pixels)[i].r);
			file.put(((Color*)pixels)[i].g);
			file.put(((Color*)pixels)[i].b);
			file.put(((Color*)pixels)[i].a);
		}
	}

	file.close();
}

void Bitmap::allocate(int w, int h)
{
	FlogAssert(bytesPerPixel == 3 || bytesPerPixel == 4, "bytes per pixel not 3 or 4");
	if(w * h > allocated){
		if(pixels){
			LogDebug("delete pixels");
			//delete[] pixels;
			free(pixels);
		}
		//LogDebug("new pixels");
		//pixels = new Color [w * h];
		pixels = malloc(bytesPerPixel * w * h);
		allocated = w * h;
	}

	this->w = w;
	this->h = h;
}

void Bitmap::clear(byte r, byte g, byte b, byte a)
{
	Color color;
	color.set(r, g, b, a);
	clear(color);
}

void Bitmap::copy(Bitmap& bitmap)
{
	allocate(bitmap.w, bitmap.h);
	memcpy(pixels, bitmap.pixels, bytesPerPixel * w * h);
}

void Bitmap::clear(Color& color)
{
	for(int i = 0; i < w * h; i++){
		if(bytesPerPixel == 3){
			((Color3i*)pixels)[i].set(color);
		}else{
			((Color*)pixels)[i] = color;
		}
	}
}

Bitmap::~Bitmap()
{
	if(pixels && freeData){
		LogDebug("delete pixels (dest)");
		//delete[] pixels;
		free(pixels);
	}
}

// only works with rgba
float Bitmap::getDistance(Bitmap& bitmap)
{
	float ret = 0;

	// assuming that a and b are the same dimensions

	byte* apx = (byte*)pixels;
	byte* bpx = (byte*)bitmap.pixels;

	for(int i = 0; i < std::min(w * h, bitmap.w * bitmap.h); i++){
		float aval = ((float)*(apx) + (float)*(apx+1) + (float)*(apx+2)) / 765.0f; /* 3 * 255 */
		float bval = ((float)*(bpx) + (float)*(bpx+1) + (float)*(bpx+2)) / 765.0f;

		ret += ((float)aval - (float)bval) * ((float)aval - (float)bval);

		bpx += bytesPerPixel;
		apx += bytesPerPixel;
	}

	return ret / (float)(w * h);
}


void Bitmap::randomize()
{
	if(bytesPerPixel != 4) return;
	Color* p = (Color*)pixels;
	for(int i = 0; i < w * h; i++){
		p->r = rand() % 256;
		p->g = rand() % 256;
		p->b = rand() % 256;
		p->a = 255;
		p++;
	}
}
