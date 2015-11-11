/*
 * SSG VideoPlayer
 *  Multi process video player for windows.
 *
 * Copyright (c) 2010-2015 Safer Society Group Sweden AB
 * All Rights Reserved.
 *
 * This file is part of SSG VideoPlayer.
 *
 * SSG VideoPlayer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * SSG VideoPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SSG VideoPlayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AUDIOHANDLER_H
#define AUDIOHANDLER_H

#include <functional>
#include <memory>

#include "avlibs.h"
#include "IAudioDevice.h"
#include "TimeHandler.h"
#include "Frame.h"

typedef std::shared_ptr<class AudioHandler> AudioHandlerPtr;

class AudioHandler
{
	public:
	virtual int getSampleRate() = 0;
	virtual int getChannels() = 0;
	virtual int getBitRate() = 0;
	virtual const char* getCodec() = 0;
	virtual int decode(AVPacket& packet, AVStream* stream, double timeWarp, FramePtr frame, int& frameFinished) = 0;
	virtual int fetchAudio(int16_t* data, int nSamples) = 0;
	virtual void onSeek() = 0;
	virtual void clearQueue() = 0;
	virtual void discardQueueUntilTs(double ts) = 0;
	virtual int getAudioQueueSize() = 0;
	virtual void EnqueueAudio(const std::vector<Sample>& samples) = 0;

	virtual void SetVolume(float volume) = 0;
	virtual void SetMute(bool mute) = 0;
	virtual void SetQvMute(bool qvMute) = 0;

	static AudioHandlerPtr Create(AVCodecContext* aCodecCtx, IAudioDevicePtr device, TimeHandlerPtr timeHandler); 
};

#endif
