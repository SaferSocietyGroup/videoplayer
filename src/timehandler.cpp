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

#include "timehandler.h"

#include <iostream>
//#define DEBUG
#include "log.h"
#include "avlibs.h"

TimeHandler::TimeHandler()
{
	time = 0;
	warp = 1;
	paused = true;
}

bool TimeHandler::getPaused()
{
	return paused;
}

void TimeHandler::pause()
{
	paused = true;
}

void TimeHandler::play()
{
	paused = false;
}

void TimeHandler::setTime(double t)
{
	time = t;
}

void TimeHandler::setTimeWarp(double tps)
{
	warp = tps;
}

double TimeHandler::getTimeWarp()
{
	return warp;
}

double TimeHandler::getTime()
{
	return time;
}

void TimeHandler::addTime(double t)
{
	if(!paused)
		time += t * warp;
}
