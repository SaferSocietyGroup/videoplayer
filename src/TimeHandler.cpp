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

#include "TimeHandler.h"

#include <iostream>
#include "avlibs.h"

class CTimeHandler : public TimeHandler
{
	public:
	IAudioDevicePtr audioDevice;

	double time = 0.0;
	double warp = 1.0;
	bool paused = true;

	CTimeHandler(IAudioDevicePtr audioDevice) : audioDevice(audioDevice)
	{
	}

	bool GetPaused()
	{
		audioDevice->Lock(true);
		bool ret = paused;
		audioDevice->Lock(false);
		return ret;
	}

	void Pause()
	{
		audioDevice->Lock(true);
		paused = true;
		audioDevice->Lock(false);
	}

	void Play()
	{
		audioDevice->Lock(true);
		paused = false;
		audioDevice->Lock(false);
	}

	void SetTime(double t)
	{
		audioDevice->Lock(true);
		time = t;
		audioDevice->Lock(false);
	}

	void SetTimeWarp(double tps)
	{
		audioDevice->Lock(true);
		warp = tps;
		audioDevice->Lock(false);
	}

	double GetTimeWarp()
	{
		audioDevice->Lock(true);
		double ret = warp;
		audioDevice->Lock(false);
		return ret;
	}

	double GetTime()
	{
		audioDevice->Lock(true);
		double ret = time;
		audioDevice->Lock(false);
		return ret;
	}

	void AddTime(double t)
	{
		audioDevice->Lock(true);
		if(!paused)
			time += t;
		audioDevice->Lock(false);
	}
};

TimeHandlerPtr TimeHandler::Create(IAudioDevicePtr audioDevice)
{
	return std::make_shared<CTimeHandler>(audioDevice);
}
