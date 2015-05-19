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

#ifndef VIDEO_H
#define VIDEO_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "audiohandler.h"
#include "timehandler.h"

#include "avlibs.h"
#include "stream.h"

class Frame
{
public:
	Frame(AVFrame* f = 0, double pts = 0, int fn = 0);
	AVFrame* avFrame;
	int frame;
	double pts;
};

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
	typedef std::function<void(const Sample* buffer, int size)> AudioCallback;

	virtual ~Video(){};

	virtual Frame fetchFrame() = 0;
	virtual void adjustTime() = 0;
	virtual void frameToSurface(Frame frame, uint8_t* buffer, int w = 0, int h = 0, int sw = 0, int sh = 0) = 0;
	virtual bool seek(int frame, bool exact = false) = 0;
	virtual bool step() = 0;
	virtual bool stepBack() = 0;
	virtual void tick() = 0;
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
	virtual int getPosition() = 0;
	virtual float getAspect() = 0;
	virtual std::string getIFrames() = 0;
	virtual std::string getVideoCodecName() = 0;
	virtual std::string getFormat() = 0;
	virtual int getVideoBitRate() = 0;
	virtual void setIFrames(std::string iframes) = 0;
	virtual void setPlaybackSpeed(double speed) = 0;
	virtual Frame getCurrentFrame() = 0;

	virtual int getSampleRate() = 0;
	virtual int getNumChannels() = 0;
		
	virtual void pause() = 0;
	virtual bool getPaused() = 0;
	virtual void addTime(double t) = 0;
	virtual double getTime() = 0;
	
	static VideoPtr Create(StreamPtr s, ErrorCallback errorHandler, class IAudioDevice* audioDevice, 
		int frameQueueSize, const std::string& kf = "");
};

#endif
