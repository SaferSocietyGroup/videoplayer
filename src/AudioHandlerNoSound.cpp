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

#include "AudioHandlerNoSound.h"
#include "Flog.h"

class CAudioHandlerNoSound : public AudioHandlerNoSound
{
	public:
	TimeHandlerPtr timeHandler;
	IAudioDevicePtr device;

	CAudioHandlerNoSound(IAudioDevicePtr audioDevice, TimeHandlerPtr timeHandler)
	{
		this->timeHandler = timeHandler;
		this->device = audioDevice;
	}

	int getAudioQueueSize()
	{
		return 0;
	}
	
	void clearQueue()
	{
	}

	int fetchAudio(int16_t* data, int nSamples)
	{
		float time = 1.0 / (double)device->GetRate() * (double)nSamples;
		timeHandler->AddTime(time);
		return 0;
	}

	int decode(AVPacket& packet, AVStream* stream, double timeWarp, bool addToQueue)
	{
		return 0;
	}
	
	void SetVolume(float volume)
	{
	}

	void SetMute(bool mute)
	{
	}

	void SetQvMute(bool qvMute)
	{
	}
	
	void onSeek()
	{
	}

	int getSampleRate()
	{
		return 0;
	}

	int getChannels()
	{
		return 0;
	}

	int getBitRate()
	{
		return 0;
	}

	const char* getCodec()
	{
		return "none";
	}
};

AudioHandlerPtr AudioHandlerNoSound::Create(IAudioDevicePtr device, TimeHandlerPtr timeHandler)
{
	return std::make_shared<CAudioHandlerNoSound>(device, timeHandler);
}
