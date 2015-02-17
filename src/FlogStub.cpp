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

#include "Flog.h"
#include <stdarg.h>

int Flog_SeverityToIndex(Flog_Severity severity)
{
	for(int i = 0; i < 8; i++){
		if(((int)severity >> i) & 1){
			return i;
		}
	}

	return 8;
}

const char* Flog_SeverityToString(Flog_Severity severity)
{
	static const char *severityString[] = { "D1", "D2", "D3", "VV", "II", "WW", "EE", "FF", "??" };
	return severityString[Flog_SeverityToIndex(severity)];
}

void Flog_Log(const char* file, uint32_t lineNumber, Flog_Severity severity, const char* format, ...)
{
	va_list fmtargs;
	char buffer[4096];

	buffer[sizeof(buffer) - 1] = '\0';

	va_start(fmtargs, format);
	vsnprintf(buffer, sizeof(buffer) - 1, format, fmtargs);
	va_end(fmtargs);
	
	printf("[%s] %s:%d %s\r\n", Flog_SeverityToString(severity), file, lineNumber, buffer);
	fflush(stdout);
}
