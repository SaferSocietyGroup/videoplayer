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

#ifndef SAMPLEQUEUE_H
#define SAMPLEQUEUE_H

#include <queue>
#include <SDL.h>
#include <cstdint>

#pragma pack(push)
#pragma pack(1)
struct Sample {
	int16_t chn[2];
	double ts = 0; 
	int64_t frameIndex = 0;
};
#pragma pack(pop)

class SampleQueue: public std::queue<Sample>
{
	public:
	void clear();
};

#endif
