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

#ifndef AUDIOHANDLER_H
#define AUDIOHANDLER_H

#include <functional>
#include <memory>

class AudioHandler;
typedef std::shared_ptr<AudioHandler> AudioHandlerPtr;

#include "avlibs.h"
#include "Sample.h"

class AudioHandler
{
	public:
	virtual int getSampleRate() = 0;
	virtual int getChannels() = 0;
	virtual int getBitRate() = 0;
	virtual const char* getCodec() = 0;
	virtual int decode(AVPacket& packet, AVStream* stream, double timeWarp) = 0;

	static AudioHandlerPtr Create(AVCodecContext* aCodecCtx, std::function<void(const Sample* buffer, int size)> audioCb, int channels, int freq); 
};

#endif
