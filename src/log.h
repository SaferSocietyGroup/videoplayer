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

#ifndef LOG_H
#define LOG_H

#include "flog.h"

#define LogExp FlogExpD

#define LogDebug FlogD
#define LogVerbose FlogV
#define LogInfo FlogI
#define LogWarning FlogW
#define LogError FlogE

#if 0
enum LogLevel
{
	LDebug,
	LVerbose,
	LInfo,
	LWarning,
	LError,
	LNumLevels
};

#include <sstream>
#include <string>

void log(std::string str, LogLevel level);

#ifdef DEBUG
#define LogDebug(__string) Log(__string << "\t\t\t\t\t(in " << __FUNCTION__ << ")", LDebug);
#else
#define LogDebug(__string)
#endif

#define LogVerbose(__string) Log(__string, LVerbose);
#define LogInfo(__string) Log(__string, LInfo);
#define LogWarning(__string) Log(__string, LWarning);
#define LogError(__string) Log(__string, LError);

#define LogExp(__var) LogDebug("Expression: " << #__var << " = " << ( __var ))

#define Log(__string, __errorlevel) do{ \
        std::ostringstream __tmpStr; \
        __tmpStr << __string; \
        log(__tmpStr.str(), __errorlevel); \
} while(0);

#define ABlack "0"
#define ARed "1"
#define AGreen "2"
#define AYellow "3"
#define ABlue "4"
#define AMagenta "5"
#define ACyan "6"
#define AWhite "7"

#define NO_ANSI_COLORS

#ifndef NO_ANSI_COLORS

#define AnsiColorIn(__color) "\033[9" __color "m"
#define AnsiColorOut "\033[39m"
#define AnsiColorBGIn(__color) "\033[4" __color "m"
#define AnsiColorBGOut "\033[49m"

#define AnsiBoldIn "\033[1m"
#define AnsiBoldOut "\033[22m"
#define AnsiUnderlineIn "\033[4m" 
#define AnsiUnderlineOut "\033[24m"
#define AnsiInvertIn "\033[7m" 
#define AnsiInvertOut "\033[27m"

#define BannerIn "\033[40m \033[44m \033[104m \033[107m \033[49m "
#define BannerOut " \033[107m \033[104m \033[44m \033[40m \033[49m"

#else

#define AnsiColorIn(__color) ""
#define AnsiColorOut ""
#define AnsiColorBGIn(__color) ""
#define AnsiColorBGOut ""

#define AnsiBoldIn ""
#define AnsiBoldOut ""
#define AnsiUnderlineIn ""
#define AnsiUnderlineOut ""
#define AnsiInvertIn ""
#define AnsiInvertOut ""

#define BannerIn "=== "
#define BannerOut " ==="

#endif

#define AnsiColor(__str, __color) AnsiColorIn(__color) __str AnsiColorOut
#define AnsiBold(__str) AnsiBoldIn __str AnsiBoldOut
#define AnsiUnderline(__str) AnsiUnderlineIn __str AnsiUnderlineOut
#define AnsiInvert(__str) AnsiInvertIn __str AnsiInvertOut

#endif
#endif
