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

#include "color.h"

void Color::set(byte r, byte g, byte b, byte a)
{
	this->r = r;
	this->g = g;
	this->b = b;
	this->a = a;
}

Color::Color(byte r, byte g, byte b, byte a)
{
	set(r, g, b, a);
}

void Color::set(const Color3i& c3)
{
	set(c3.r, c3.g, c3.b, 255);
}

void Color3i::set(byte r, byte g, byte b)
{
	this->r = r;
	this->g = g;
	this->b = b;
}

Color3i::Color3i(byte r, byte g, byte b)
{
	set(r, g, b);
}
	
void Color3i::set(const Color& color)
{
	set(color.r, color.g, color.b);
}

void Color3f::set(float r, float g, float b, float a)
{
	this->r = r;
	this->g = g;
	this->b = b;
	this->a = a;
}

Color3f::Color3f(float r, float g, float b, float a)
{
	set(r, g, b, a);
}
