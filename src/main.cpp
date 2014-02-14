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

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
//#define WRAP_ACCESS_VIOLATION

#include "common.h"

#include "player.h"
#include "flog.h"

#include <iostream>
#include <sstream>
#include <cstdint>

int main_wrap(int argc, char** argv)
{
	std::cout << "starting video player" << std::endl;

	FlogAssert(argc == 3, "expected 2 parameters: [with a base-name for a memory mapped file] [window handle of host]");

	Sleep(1000);
	IpcMessageQueuePtr ipc = IpcMessageQueue::Open(argv[1]);

	std::stringstream ss(argv[2]);
	intptr_t handle;

	ss >> handle;
	
	FlogD("starting player");
	Player player;
	player.Run(ipc, handle);

	return 0;
}

#ifdef WRAP_ACCESS_VIOLATION

// Catch access violation exceptions

void SignalHandler(int signal)
{
	FlogF("Access violation!");
}

int main(int argc, const char** argv)
{
	__try{
		main_wrap(argc, argv);
	}
	__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		Flog_Log(__FILE__, __LINE__, Flog_SFatal, "Access violation!");
		exit(-100);
	}

	return 0;
}

#else

#ifdef _USEWINMAIN

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	char* argv[2] = {"lala", "test"};
	main_wrap(2, argv);
	return 0;
}

#else

// must not be char** without any const pointers because of SDLmain
int main(int argc, char** argv)
{
	return main_wrap(argc, argv);
}

#endif

#endif
