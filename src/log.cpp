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

#if 0
#include "log.h"

#include <iostream>

void log(std::string str, LogLevel level)
{
	std::string tmp[LNumLevels] = {
		AnsiColor("DD", AMagenta),
		AnsiColor("VV", ACyan),
		AnsiColor("II", AWhite),
		AnsiColor("WW", AYellow),
		AnsiColor("EE", ARed)
	};

	std::cout << "[" << tmp[level % LNumLevels] << "] " << str << std::endl;
}

#endif
