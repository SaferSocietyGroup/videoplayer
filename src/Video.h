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

#ifndef VIDEO_H
#define VIDEO_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "avlibs.h"
#include "Stream.h"
#include "IAudioDevice.h"

class Video;
typedef std::shared_ptr<Video> VideoPtr;

class Video
{
	public:
	enum Error {
		// proper errors
		ENoError,
		EFile,
		EVideoCodec,
		EStreamInfo,
		EStream,
		EDemuxing,
		EDecoding,
		ESeeking,

		// messages rather than errors (TODO: rename enum / callback?)
		EEof,
		EUnloadedFile
	};
	
	typedef std::function<void(Error, const std::string&)> ErrorCallback;

	virtual ~Video(){};
	
	virtual int fetchAudio(int16_t* data, int nSamples) = 0;
	virtual bool update(double deltaTime) = 0;
	virtual void updateOverlay(uint8_t** pixels, const uint16_t* pitches, int w, int h) = 0;

	virtual bool seek(double ts) = 0;
	virtual bool step() = 0;
	virtual bool stepBack() = 0;
	virtual void play() = 0;
	virtual int getWidth() = 0;
	virtual int getHeight() = 0;
	virtual int getDurationInFrames() = 0;
	virtual int getReportedDurationInFrames() = 0;
	virtual double getReportedDurationInSecs() = 0;
	virtual double getDurationInSecs() = 0;
	virtual float getFrameRate() = 0;
	virtual float getReportedFrameRate() = 0;
	virtual float getPAR() = 0;
	virtual double getPosition() = 0;
	virtual float getAspect() = 0;
	virtual std::string getVideoCodecName() = 0;
	virtual std::string getFormat() = 0;
	virtual int getVideoBitRate() = 0;
	virtual void setPlaybackSpeed(double speed) = 0;

	virtual int getSampleRate() = 0;
	virtual int getNumChannels() = 0;
		
	virtual void pause() = 0;
	virtual bool getPaused() = 0;
	virtual double getTime() = 0;
	
	static VideoPtr Create(StreamPtr s, ErrorCallback errorHandler, IAudioDevicePtr audioDevice, int frameQueueSize);
};

#endif
