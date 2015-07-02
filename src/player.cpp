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

#include <cstdlib>

#include "player.h"
#include "video.h"
#include "common.h"
#include "ipc.h"
#include "audiohandler.h"
#include "flog.h"
#include "tools.h"
#include "filestream.h"

typedef std::pair<std::string, std::string> Message;

void Player::AudioCallback(void *vMe, Uint8 *stream, int len)
{
	((Player*)vMe)->HandleAudio(stream, len);
}

void Player::HandleAudio(Uint8* stream, int len)
{
	bool noSamples = samples.empty();
	Sample smp;
	double vt = 0.0;
		
	if(video != 0)
		vt = video->getTime();

	// on a seek, audio might be far ahead of video,
	// so on a seek we skip audio until it matches
	// the current video time

	if(audioSkip){
		while(!samples.empty())
		{
			if(samples.front().ts < vt){
				samples.pop();
			}else{
				audioSkip = false;
				break;
			}
		}
	}

	float useVolume = (qvMute || mute) ? 0.0f : volume;

	for(int i = 0; i < len; i += 4){
		if(!samples.empty()){
			Sample s = samples.front();
			samples.pop();

			for(int j = 0; j < 2; j++){
				s.chn[j] = (int16_t)((float)s.chn[j] * useVolume);
			}

			stream[i+0] = s.chn[0] & 0xff;
			stream[i+1] = (s.chn[0] >> 8) & 0xff;

			stream[i+2] = s.chn[1] & 0xff;
			stream[i+3] = (s.chn[1] >> 8) & 0xff;

			smp = s;
		}
		
		else{
			for(int i = 0; i < len; i += 4){
				stream[i] = stream[i+1] = stream[i+2] = stream[i+3] = 0;
			}
		}
	}

	// keep track of the time between audio frames
	if(!noSamples && smp.frameIndex != lastSample.frameIndex){
		audioFrameFrequency = smp.ts - lastSample.ts;
	}

	if(video != 0){
		double diff = smp.ts - vt;
	
		// add the difference between the last audio sample's timestamp and the current video time stamp
		// to the video time

		double addTime = std::max(0.0, diff);

		// if there are no samples (presumably the video has no audio track)
		// or, if the rate at which we get audio frames is too low 
		// (and the audio time is not too far ahead of the video time),
		// increase the video time by the audio rate instead

		if(noSamples || (smp.frameIndex == lastSample.frameIndex && vt < smp.ts + audioFrameFrequency)){
			addTime = 1.0 / (double)freq * (double)len / 4;
		}

		video->addTime(addTime);
	}

	if(!noSamples)
		lastSample = smp;
}
	
int AudioFallbackThread(void* data)
{
	FlogD("starting audio fallback thread");

	Player* me = (Player*)data;

	SDL_AudioSpec fmt = me->audioFormat;

	int tick = 0;
	int maxSize = 10000;
	uint8_t buffer[maxSize];

	while(me->running){
		int elapsed = SDL_GetTicks() - tick;
		tick = SDL_GetTicks();

		bool paused = false;

		SDL_LockMutex(me->noAudioMutex);

		if(me->video != 0)
			paused = me->video->getPaused();

		if(!paused){
			int size = (float)(fmt.freq * fmt.channels * 2) / 1000.0f * (float)elapsed;
			Player::AudioCallback(data, buffer, std::min(size, maxSize));
		}

		SDL_UnlockMutex(me->noAudioMutex);

		SDL_Delay(8);
	}

	return 0;
}

void Player::emptyQueue()
{
	LockAudio();
	samples.clear();
	UnlockAudio();
}
	
void Player::LockAudio()
{
	if(audioOutputEnabled)
		SDL_LockAudio();
	else
		SDL_LockMutex(noAudioMutex);
}

void Player::UnlockAudio()
{
	if(audioOutputEnabled)
		SDL_UnlockAudio();
	else
		SDL_UnlockMutex(noAudioMutex);
}

int Player::getBlockSize()
{
	return audioFormat.samples;
}

int Player::getSampleCount()
{
	LockAudio();
	int size = samples.size();
	UnlockAudio();
	return size;
}

void Player::EnqueueSamples(const Sample* buffer, int size)
{
	LockAudio();
	for(int i = 0; i < size; i++)
		samples.push(buffer[i]);
	UnlockAudio();
}

int Player::getSampleRate()
{
	return audioFormat.freq;
}

int Player::getChannelCount()
{
	return audioFormat.channels;
}

void Player::PauseAudio(bool val)
{
	if(!audioOutputEnabled)
		return;

	if(!val){
		InitAudio();
		SDL_PauseAudio(false);
	}

	else CloseAudio();
}

void Player::CloseAudio()
{
	if(!initialized) return;
	FlogD("closing audio");

	SDL_CloseAudio();
	initialized = false;
}

void Player::InitAudio()
{
	if(initialized) return;
	FlogD("inititalizing audio");

	SDL_AudioSpec fmt;

	memset(&fmt, 0, sizeof(SDL_AudioSpec)); 
	memset(&audioFormat, 0, sizeof(SDL_AudioSpec)); 

	fmt.freq = 48000;
	fmt.format = AUDIO_S16LSB;
	fmt.channels = 2;
	fmt.samples = 1024;
	fmt.callback = AudioCallback;
	fmt.userdata = this;

	if(SDL_OpenAudio(&fmt, &audioFormat) != 0){
		FlogI("Could not open audio. Using fallback.");
		audioOutputEnabled = false;

		noAudioThread = SDL_CreateThread(AudioFallbackThread, (void*)this);
		noAudioMutex = SDL_CreateMutex();

		// the set the "null" audio parameters to 48 KHz 16 bit stereo
		audioFormat = fmt;		

		initialized = true;
		return;
	}

	audioOutputEnabled = true;

	if(audioFormat.format != fmt.format){
		FlogW("audio format mismatch");
	}else{
		FlogI("audio format accepted");
	}

	initialized = true;

	freq = audioFormat.freq;
}
	
void Player::SetDims(int nw, int nh, int vw, int vh)
{
	FlogExpD(nw);
	FlogExpD(nh);
	FlogExpD(vw);
	FlogExpD(vh);

	nw = std::max(std::min(nw, 1920), 64);
	nh = std::max(std::min(nh, 1080), 64);

	// Keep aspect ratio
	float zoom = std::min(std::min((float)nw / (float)vw, (float)nh / (float)vh), 1.0f);

	w = (float)vw * zoom;
	h = (float)vh * zoom;
	
	FlogExpD(w);
	FlogExpD(h);
}

int Player::MessageHandlerThread(void* instance)
{
	Player* me = (Player*)instance;

	while(me->running){
		std::string type, message;
		while(me->ipc->ReadMessage(type, message, 10)){
			SDL_LockMutex(me->messageQueueMutex);
	
			// clear the message queue on a load
			if(type == "load"){
				while(!me->messageQueue.empty()){
					me->messageQueue.pop();
				}
			}

			me->messageQueue.push(std::pair<std::string, std::string>(type, message));

			SDL_UnlockMutex(me->messageQueueMutex);
		}
	}

	return 0;
}

void Player::Run(IPC& ipc)
{
	char env[] = "SDL_AUDIODRIVER=dsound";
	putenv(env);
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO);

	running = true;

	this->ipc = &ipc;

	messageQueueMutex = SDL_CreateMutex();
	messageQueueThread = SDL_CreateThread(MessageHandlerThread, (void*)this);

	initialized = false;

	mute = qvMute = false;
	volume = 1;

	bool quickViewPlayer = false;

	InitAudio();	

	w = h = 0;
	freq = 48000;

	uint32_t keepAliveTimer = SDL_GetTicks();

	while(running){
		while(true){
			std::string type, message;

			SDL_LockMutex(messageQueueMutex);

			bool wasMessage = !messageQueue.empty();

			if(wasMessage){
				type = messageQueue.front().first;
				message = messageQueue.front().second;
				messageQueue.pop();
			}

			SDL_UnlockMutex(messageQueueMutex);

			if(!wasMessage){
				SDL_Delay(video && !video->getPaused() ? 8 : 50);
				break;
			}

			if(type == "keepalive")
				keepAliveTimer = SDL_GetTicks();

			if(type == "load" || type == "lfsc_load"){
				emptyQueue();
				StreamPtr s;

				FlogD("trying to load: " << message << " (" << type << ")");

				try {
					video = 0;

					if(type == "load"){
						FileStreamPtr fs = FileStream::Create();
						fs->Open(message, false);
						s = fs;
					}

					else {
						s = lfsc->Open(Tools::StrToWstr(message));
						s->Seek(0, SEEK_SET);
					}

					auto errorCallback = [&](Video::Error error, const std::string& msg){
						if(error < Video::EEof)
							ipc.WriteMessage("error", msg);
						else
							ipc.WriteMessage(error == Video::EEof ? "eof" : "unloaded", msg);
					};

					video = Video::Create(s, errorCallback, this, quickViewPlayer ? 5 : 16);
				}

				catch (StreamEx ex)
				{
					FlogE("failed to load file: " << ex.what());
					ipc.WriteMessage("error", ex.what());
				}

				if(video){
					if(w && h)
						SetDims(w, h, video->getWidth(), video->getHeight());
					else
						SetDims(video->getWidth(), video->getHeight(), video->getWidth(), video->getHeight());
					FlogD("loaded");
				}else{
					FlogD("not loaded");
				}
			}
			
			else if(type == "setquickviewplayer"){
				quickViewPlayer = (message == "true");
			}
			
			else if(type == "lfsc_connect"){
				lfsc = Lfscpp::Create();
				try {
					lfsc->Connect(Tools::StrToWstr(message), 3000);
				}

				catch (StreamEx ex)
				{
					FlogE("failed to load file: " << ex.what());
					ipc.WriteMessage("error", ex.what());
				}
			}

			else if(type == "quit"){
				running = false;
			}

			if(video){
				std::stringstream s(message);
				
				if(type == "qvmute"){
					FlogD(type);
					qvMute = (message == "true");
				}

				if(type == "mute"){
					FlogD(type);
					FlogExpD(message == "true");
					mute = (message == "true");
				}

				if(type == "setplaybackspeed"){
					float speed = 1;
					s >> speed;
					FlogExpD(speed);
					video->setPlaybackSpeed(speed);
				}

				else if(type == "volume"){
					FlogD(type);
					float vol;
					s >> vol;
					FlogExpD(vol);
					vol = std::max(std::min(1.0f, vol), 0.0f);
					volume = log10(vol * 9 + 1);
					FlogExpD(volume);
				}

				else if(type == "unload")
				{
					FlogD(type);
					if(video)
						video = 0;
					else
						ipc.WriteMessage("unloaded", message);
				}

				else if(type == "setkeyframes"){
					video->setIFrames(message);
				}

				else if(type == "getkeyframes"){
					ipc.WriteMessage("keyframes", video->getIFrames());
				}

				else if(type == "play") {
					PauseAudio(false);
					video->play(); 
					FlogD("play");
				}

				else if(type == "pause") {
					video->pause(); 
					PauseAudio(true);
					FlogD("pause");	
				}

				else if(type == "getinfo") {
					ipc.WriteMessage("dimensions", Str(video->getWidth() << " " << Str(video->getHeight())));
					ipc.WriteMessage("videocodec", video->getVideoCodecName());
					ipc.WriteMessage("format", video->getFormat());
					ipc.WriteMessage("duration_in_frames", Str(video->getDurationInFrames()));
					ipc.WriteMessage("reported_duration_in_frames", Str(video->getReportedDurationInFrames()));
					ipc.WriteMessage("framerate", Str(video->getFrameRate()));
					ipc.WriteMessage("reported_framerate", Str(video->getReportedFrameRate()));					
					ipc.WriteMessage("aspectratio", Str(video->getAspect()));
					ipc.WriteMessage("reported_length_in_secs", Str(video->getReportedDurationInSecs()));
					ipc.WriteMessage("length_in_secs", Str(video->getDurationInSecs()));
					ipc.WriteMessage("sample_rate", Str(video->getSampleRate()));
					ipc.WriteMessage("audio_num_channels", Str(video->getNumChannels()));
				}

				else if(type == "step"){
					FlogD("step " << message);
					if(message == "back") video->stepBack();
					else video->step();
				}
				
				else if(type == "seek") {
					float t;
					s >> t;
					FlogD("seek " << t);
					emptyQueue();

					t = std::min(std::max(0.0f, t), 1.0f);

					video->seek(t * (float)video->getDurationInFrames(), false);
					audioSkip = true;
				}

				else if(type == "setdims"){
					int w, h;
					FlogD("setdims " << w << " " << h);

					FlogExpD(message);

					s >> w;
					s >> h;

					SetDims(w, h, video->getWidth(), video->getHeight());
				}

				else if(type == "snapshot"){
					FlogD("got a snapshot");
					char* buffer = ipc.GetWriteBuffer();

					uint16_t w = video->getWidth(), h = video->getHeight();

					*((uint16_t*)buffer) = w;
					*(((uint16_t*)buffer) + 1) = h;

					Frame currentFrame = video->getCurrentFrame();

					if(currentFrame.avFrame){
						FlogD("snapshot w: " << w << " h: " << h);
						video->frameToSurface(currentFrame, (uint8_t*)buffer + 4, w, h);
						ipc.ReturnWriteBuffer("snapshot", &buffer, w * h * 3 + 4);
						FlogD("sent snapshot data");
					} else {
						FlogW("no frame to send");
					}
				}
			}

			// !video
			else {
				if(type == "getkeyframes"){
					ipc.WriteMessage("keyframes", "error");
				}
			}
		}

		if(video){
			video->tick();
			video->adjustTime();

			Frame frame = video->fetchFrame();

			if(frame.avFrame){
				//FlogD("got a frame");
				char* buffer = ipc.GetWriteBuffer();
				*((uint16_t*)buffer) = w;
				*(((uint16_t*)buffer) + 1) = h;

				video->frameToSurface(frame, (uint8_t*)buffer + 4, w, h);
				ipc.ReturnWriteBuffer("frame", &buffer, w * h * 3 + 4);

				ipc.WriteMessage("position", Str((float)video->getPosition() / (float)video->getDurationInFrames()));
			}else{
				//FlogD("no frame");
			}
		}

		// 32bit int wraps every ~58 days, reset it if this has happened
		if(SDL_GetTicks() < keepAliveTimer)
			keepAliveTimer = SDL_GetTicks();

		// Send keepalive every ~5 seconds
		if(SDL_GetTicks() - keepAliveTimer > 5000){
			ipc.WriteMessage("keepalive", "");
		}

		// No keepalive message for ~30 seconds, die
		if(SDL_GetTicks() - keepAliveTimer > 60000){
			FlogD("dying bye bye");
			running = false;
		}
	}

	SDL_WaitThread(messageQueueThread, NULL);
	SDL_DestroyMutex(messageQueueMutex);

	SDL_Quit();

	video = 0;

	FlogD("exiting");
}
