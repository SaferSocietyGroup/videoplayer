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

/*#pragma GCC system_header*/

#ifndef FLOG_H
#define FLOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
	Flog_SDebug1 = 1,
	Flog_SDebug2 = 2,
	Flog_SDebug3 = 4,
	Flog_SVerbose = 8,
	Flog_SInfo = 16,
	Flog_SWarning = 32,
	Flog_SError = 64,
	Flog_SFatal = 128
} Flog_Severity;

#define Flog_SAll 255
#define Flog_DefaultPort 13000

/*void Flog_Init(const char* applicationName);

int Flog_AddTargetFile(const char* filename, uint8_t filter);
void Flog_AddTargetStream(FILE* stream, uint8_t filter, int useAnsiColors);
int Flog_AddTargetServer(const char* address, uint16_t port, uint8_t filter);*/

void Flog_Log(const char* file, uint32_t lineNumber, Flog_Severity severity, const char* format, ...);
void Flog_SetTargetFile(FILE* file);

#define FlogD FlogD3

#ifndef __cplusplus

#define FlogD1(...) Flog_Log(__FILE__, __LINE__, Flog_SDebug1, __VA_ARGS__)
#define FlogD2(...) Flog_Log(__FILE__, __LINE__, Flog_SDebug2, __VA_ARGS__)
#define FlogD3(...) Flog_Log(__FILE__, __LINE__, Flog_SDebug3, __VA_ARGS__)

#define FlogV(...) Flog_Log(__FILE__, __LINE__, Flog_SVerbose, __VA_ARGS__)
#define FlogI(...) Flog_Log(__FILE__, __LINE__, Flog_SInfo, __VA_ARGS__)
#define FlogW(...) Flog_Log(__FILE__, __LINE__, Flog_SWarning, __VA_ARGS__)
#define FlogE(...) Flog_Log(__FILE__, __LINE__, Flog_SError, __VA_ARGS__)
#define FlogF(...) Flog_Log(__FILE__, __LINE__, Flog_SFatal, __VA_ARGS__)
#define FlogDie(...) Flog_Log(__FILE__, __LINE__, Flog_SFatal, __VA_ARGS__); exit(1)
#define FlogAssert(__val, ...) if(!(__val)){Flog_Log(__FILE__, __LINE__, Flog_SFatal, __VA_ARGS__); exit(1); }

#else
}

/* C++ Wrapper */

#include <sstream>

#define FlogD1(__string) Log(__string, Flog_SDebug1)
#define FlogD2(__string) Log(__string, Flog_SDebug2)
#define FlogD3(__string) Log(__string, Flog_SDebug3)
#define FlogV(__string) Log(__string, Flog_SVerbose)
#define FlogI(__string) Log(__string, Flog_SInfo)
#define FlogW(__string) Log(__string, Flog_SWarning)
#define FlogE(__string) Log(__string, Flog_SError)
#define FlogF(__string) Log(__string, Flog_SFatal)
#define FlogDie(__string) Log(__string, Flog_SFatal); exit(1)
#define FlogAssert(__val, __string) if(!(__val)){FlogDie(__string);}

#define FlogExp(__var, __severity) Log("Expression: " << #__var << " = " << ( __var ), __severity)

#define FlogExpD FlogExpD3
#define FlogExpD1(__var) FlogExp(__var, Flog_SDebug1)
#define FlogExpD2(__var) FlogExp(__var, Flog_SDebug2)
#define FlogExpD3(__var) FlogExp(__var, Flog_SDebug3)
#define FlogExpV(__var) FlogExp(__var, Flog_SVerbose)
#define FlogExpI(__var) FlogExp(__var, Flog_SInfo)
#define FlogExpW(__var) FlogExp(__var, Flog_SWarning)
#define FlogExpE(__var) FlogExp(__var, Flog_SError)
#define FlogExpF(__var) FlogExp(__var, Flog_SFatal)

#define Log(__string, __severity) do{ \
        std::ostringstream __tmpStr; \
        __tmpStr << __string; \
	Flog_Log(__FILE__, __LINE__, __severity, "%s", __tmpStr.str().c_str());\
} while(0);

#endif

#endif
