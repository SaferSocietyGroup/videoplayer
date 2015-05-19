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

#ifndef PLAYER_H
#define PLAYER_H

#include <SDL.h>

#include "iaudiodevice.h"
#include "ipc.h"
#include "samplequeue.h"
#include "video.h"
#include "lfscpp.h"

#include <queue>

class Player : public IAudioDevice
{
	SampleQueue samples;

	void InitAudio();
	void CloseAudio();
	void SetDims(int nw, int nh, int vw, int vh);
	static int MessageHandlerThread(void* instance);
	
	// IAudioDevice
	void emptyQueue();
	int getBlockSize();
	int getSampleCount();
	int getSampleRate();
	int getChannelCount();
	void EnqueueSamples(const Sample* buffer, int size);

	bool initialized;
	int w, h;
	IPC* ipc;
		
	float volume;
	bool mute, qvMute, audioOutputEnabled;
	bool running;

	SDL_AudioSpec audioFormat;

	SDL_Thread* messageQueueThread;
	SDL_mutex* messageQueueMutex;
	std::queue<std::pair<std::string, std::string>> messageQueue;
	LfscppPtr lfsc = 0;

	SDL_TimerID noAudioTimer;

	public:
	VideoPtr video;
	int freq;

	void PauseAudio(bool val);
	void Run(IPC& ipc);
	static void AudioCallback(void *me, Uint8 *stream, int len);
};

#endif
