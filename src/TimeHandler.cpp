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

//#define DEBUG

#include <iostream>

#include "TimeHandler.h"
#include "AvLibs.h"

TimeHandler::TimeHandler()
{
	time = 0;
	warp = 1;
	paused = true;
	mutex = SDL_CreateMutex();
}

TimeHandler::~TimeHandler()
{
	SDL_DestroyMutex(mutex);
}

void TimeHandler::lock()
{
	SDL_LockMutex(mutex);
}

void TimeHandler::unlock()
{
	SDL_UnlockMutex(mutex);
}

bool TimeHandler::getPaused()
{
	lock();
	bool ret = paused;
	unlock();
	return ret;
}

void TimeHandler::pause()
{
	lock();
	paused = true;
	unlock();
}

void TimeHandler::play()
{
	lock();
	paused = false;
	unlock();
}

void TimeHandler::setTime(double t)
{
	lock();
	time = t;
	unlock();
}

void TimeHandler::setTimeWarp(double tps)
{
	lock();
	warp = tps;
	unlock();
}

double TimeHandler::getTimeWarp()
{
	lock();
	double ret = warp;
	unlock();
	return ret;
}

double TimeHandler::getTime()
{
	lock();
	double ret = time;
	unlock();
	return ret;
}

void TimeHandler::addTime(double t)
{
	lock();
	if(!paused)
		time += t * warp;
	unlock();
}
