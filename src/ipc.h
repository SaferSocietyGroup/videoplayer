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

#ifndef IPC_H
#define IPC_H

#include "common.h"

extern "C" {
#include "mmap.h"
}

struct ReadBuffer {
	const char* data;
	int dataLen;
	std::string type;
};

class IPC
{
	public:
	IPC(std::string name, bool isHost = false);

	bool ReadMessage(std::string& type, std::string& message, int timeout = 5); // 5 ms timeout per default
	bool WriteMessage(std::string type, std::string message, int timeout = 1000);

	ReadBuffer GetReadBuffer(int timeout);
	void ReturnReadBuffer(ReadBuffer buffer);

	char* GetWriteBuffer(int timeout = INFINITE);
	void ReturnWriteBuffer(std::string type, char** buffer, int len);

	private:
	Mmap* readQueue, *writeQueue;
	HANDLE readLock, writeLock;
};

#endif
