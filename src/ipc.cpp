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

#include "ipc.h"
#include "log.h"
#include "tools.h"

extern "C" {
#include "mmap.c"
}

IPC::IPC(std::string name, bool isHost)
{
	std::string writeName = name;
	std::string readName = name;

	std::string* n = isHost ? &writeName : &readName;

	n->append("_host_writer");

	if(isHost){	
		FlogExpD(writeName);
		writeQueue = Mmap_Create(writeName.c_str(), 512, 32, AM_WRITE);
		FlogExpD(readName);
		readQueue = Mmap_Create(readName.c_str(), 1920 * 1080 * 3, 32, AM_READ);
	}else{
		FlogExpD(writeName);
		writeQueue = Mmap_Open(writeName.c_str(), AM_WRITE);
		FlogExpD(readName);
		readQueue = Mmap_Open(readName.c_str(), AM_READ);
	}

	writeLock = CreateMutex(NULL, FALSE, NULL);
	readLock = CreateMutex(NULL, FALSE, NULL);
}

bool IPC::ReadMessage(std::string& type, std::string& message, int timeout)
{
	WaitForSingleObject(readLock, INFINITE);

	MmapMessageHeader header;
	//FlogD("acquiring read buffer");
	const char* buffer = Mmap_AcquireBufferR(readQueue, &header, timeout);

	bool ret = false;

	if(buffer){
		type = header.type;
		message.assign(buffer, header.length);
		//FlogExpD(header.length);
		//FlogD("returning read buffer");
		Mmap_ReturnBufferR(readQueue, &buffer);

		ret = true;
	}

	ReleaseMutex(readLock);
	return ret;
}

ReadBuffer IPC::GetReadBuffer(int timeout)
{
	WaitForSingleObject(readLock, INFINITE);

	ReadBuffer ret;
	MmapMessageHeader header;
	ret.data = Mmap_AcquireBufferR(readQueue, &header, timeout);

	if(ret.data){
		ret.type = header.type;
		ret.dataLen = header.length;
	}

	ReleaseMutex(readLock);

	return ret;
}

void IPC::ReturnReadBuffer(ReadBuffer buffer)
{
	if(buffer.data){
		Mmap_ReturnBufferR(readQueue, &buffer.data);
	}
}

bool IPC::WriteMessage(std::string type, std::string message, int timeout)
{
	WaitForSingleObject(writeLock, INFINITE);

	bool ret = false;
	char* buffer = Mmap_AcquireBufferW(writeQueue, timeout);

	if(buffer){
		memcpy(buffer, message.c_str(), message.length());
		Mmap_ReturnBufferW(writeQueue, &buffer, message.length(), type.c_str());
		ret = true;
	}

	ReleaseMutex(writeLock);
	return ret;
}

char* IPC::GetWriteBuffer(int timeout)
{
	return Mmap_AcquireBufferW(writeQueue, timeout);
}

void IPC::ReturnWriteBuffer(std::string type, char** buffer, int len)
{
	Mmap_ReturnBufferW(writeQueue, buffer, len, type.c_str());
}
