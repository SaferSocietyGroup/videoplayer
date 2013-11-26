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

#ifndef COLOR_H
#define COLOR_H

#include "tools.h"

// Disable alignment/padding for struct Pixels
// It must be exactly 4 bytes (for char* compatibility) 

#pragma pack(push)
#pragma pack(1)

class Color3i;

class Color
{
	public:
	Color(byte r = 0, byte g = 0, byte b = 0, byte a = 255);
	void set(byte r = 0, byte g = 0, byte b = 0, byte a = 255);
	void set(const Color3i& c3);

	byte r, g, b, a;
};

class Color3i
{
	public:
	Color3i(byte r = 0, byte g = 0, byte b = 0);
	void set(byte r = 0, byte g = 0, byte b = 0);
	void set(const Color& color);

	byte r, g, b;
};

#pragma pack(pop)

class Color3f
{
	public:
	Color3f(float r = 0, float g = 0, float b = 0, float a = 1.0f);
	void set(float r = 0, float g = 0, float b = 0, float a = 1.0f);

	float r, g, b, a;
};

#endif
