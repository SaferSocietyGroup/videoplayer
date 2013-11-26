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

#include <windows.h>
#include "mmap.h"
#include "flog.h"

// Implementation inspired by http://comsci.liu.edu/~murali/win32/SharedMemory.htm

#define ACTUAL_SIZE(_s) (sizeof(MmapMessageHeader) + (_s))

double GetTick()
{
	double high, low;
	FILETIME filetime;

	GetSystemTimeAsFileTime(&filetime);

	high = filetime.dwHighDateTime;
	low = filetime.dwLowDateTime;

	return (high * 4294967296.0 + low) / 10000000;
}

wchar_t* Mmap_StrToWstr(const char* str)
{
	int len = strlen(str);

	wchar_t* ret = (wchar_t*)calloc(1, (len + 1) * sizeof(wchar_t));
	for(int i = 0; i < len; i++)
		ret[i] = str[i];

	return ret;
}

Mmap* Mmap_CreateOrOpen(const char* name, uint32_t bufferSize, uint32_t bufferCount, AccessMode mode, bool open)
{
	Mmap* me = (Mmap*)calloc(1, sizeof(Mmap));

	MMAssertMsg(me, "could not allocate memory");

	me->header.count = bufferCount;
	me->header.size = bufferSize;
	me->name = Mmap_StrToWstr(name);
	me->mode = mode;

	//FlogD("Opening or creating file with name: " << name);

	if(open){
		// Open the existing "file"
		printf("opening existing shared memory file\n");
		me->file = OpenFileMappingW(
				FILE_MAP_WRITE, // read/write access
				FALSE,          // do not inherit the name
				me->name);          // name of mapping object
		
		MMAssertMsg(me->file, "Could not open shared memory file");

		// Temporarily map a view of the file to get buffer sizes etc.	
		char* buffer = (char*)MapViewOfFile(me->file, FILE_MAP_WRITE, 0, 0, sizeof(MmapHeader));
		MMAssertMsg(buffer, "could not map view of file: (%ld)", (long int)GetLastError());
		memcpy(&me->header, buffer, sizeof(MmapHeader));
		UnmapViewOfFile(buffer);

		printf("Opened file with %u buffers of size %u\n", me->header.count, me->header.size);
	}else{
		// Create a new "file"
		printf("creating new shared memory file\n");
		me->file = CreateFileMappingW(
				INVALID_HANDLE_VALUE,     // use paging file
				NULL,                     // default security
				PAGE_READWRITE,           // read/write access
				0,                        // size of memory area (high-order DWORD)
				sizeof(MmapHeader) + ACTUAL_SIZE(bufferSize) * bufferCount, // size of memory area (low-order DWORD)
				me->name);                    // name of mapping object
	}

	MMAssertMsg(me->file, "failed to create/open file mapping (%lu)", (unsigned long)GetLastError());

	// Map the views of the file
	me->buffers = (char**)calloc(sizeof(char*), me->header.count);

	char* mem = (char*)MapViewOfFile(me->file, FILE_MAP_WRITE, 0, 0, ACTUAL_SIZE(me->header.size) * me->header.count + sizeof(MmapHeader));
	//FlogExpD(ACTUAL_SIZE(me->header.size) * me->header.count + sizeof(MmapHeader));
	MMAssertMsg(mem, "could not map view of file (%lu)", (unsigned long)GetLastError());

	if(!open) memcpy(mem, me, sizeof(MmapHeader));

	for(unsigned int i = 0; i < me->header.count; i++){
		me->buffers[i] = mem + sizeof(MmapHeader) + i * ACTUAL_SIZE(me->header.size);
	}

	char tmpName[512];

	// Create the read semaphore
	sprintf(tmpName, "%s_read", name);
	wchar_t* lname = Mmap_StrToWstr(tmpName);
	MMAssertMsg(me->readSem = CreateSemaphoreW(NULL, 0, me->header.count, lname),
		"could not create semaphore (%lu)", (unsigned long)GetLastError());
	free(lname);
	
	// Create the write semaphore
	sprintf(tmpName, "%s_write", name);
	lname = Mmap_StrToWstr(tmpName);
	MMAssertMsg(me->writeSem = CreateSemaphoreW(NULL, me->header.count, me->header.count, lname),
		"could not create semaphore (%lu)", (unsigned long)GetLastError());

	free(lname);

	return me;
}

char* Mmap_AcquireBuffer(Mmap* me, int timeout)
{
	MMAssertMsg(!me->bufferInUse, "trying to acquire buffer twice");

	if(WaitForSingleObject(me->mode == AM_WRITE ? me->writeSem : me->readSem, timeout) == 0){
		//FlogD("got data");
		me->bufferInUse = true;
		//printf("me->bufferPos %d %% %d -> %d\n", 
		//	me->bufferPos, me->header.count, me->bufferPos % me->header.count);

		char* buffer = me->buffers[me->bufferPos++ % me->header.count];
		//printf("%p\n", buffer);
		return buffer;
	}
	return NULL;
}

const char* Mmap_AcquireBufferR(Mmap* me, MmapMessageHeader* header, int timeout)
{
	MMAssertMsg(me->mode == AM_READ, "trying to acquire a read buffer from a write mmap");
	const char* buffer = Mmap_AcquireBuffer(me, timeout);
	if(buffer){
		memcpy(header, buffer, sizeof(MmapMessageHeader));
		return buffer + sizeof(MmapMessageHeader);
	}
	return NULL;
}

char* Mmap_AcquireBufferW(Mmap* me, int timeout)
{
	MMAssertMsg(me->mode == AM_WRITE, "trying to acquire a write buffer from a read mmap");
	char* ret = Mmap_AcquireBuffer(me, timeout);
	return ret != NULL ? ret + sizeof(MmapMessageHeader) : NULL;
}

void Mmap_ReturnBuffer(Mmap* me)
{
	MMAssertMsg(me->bufferInUse, "trying to return buffer twice");
	me->bufferInUse = false;
	ReleaseSemaphore(me->mode == AM_WRITE ? me->readSem : me->writeSem, 1, NULL);
}

void Mmap_ReturnBufferW(Mmap* me, char** buffer, uint32_t length, const char* type)
{
	char* realBuf = (*buffer) -= sizeof(MmapMessageHeader);
	MmapMessageHeader* header = (MmapMessageHeader*)realBuf;

	header->length = length;
	strncpy(header->type, type, MMAP_MESSAGE_TYPE_LENGTH);

	Mmap_ReturnBuffer(me);
	*buffer = NULL;
}

void Mmap_ReturnBufferR(Mmap* me, const char** buffer)
{
	Mmap_ReturnBuffer(me);
	*buffer = NULL;
}

Mmap* Mmap_Open(const char* name, AccessMode mode)
{
	return Mmap_CreateOrOpen(name, 0, 0, mode, true);
}

Mmap* Mmap_Create(const char* name, uint32_t bufferSize, uint32_t bufferCount, AccessMode mode)
{
	return Mmap_CreateOrOpen(name, bufferSize, bufferCount, mode, false);
}

void Mmap_Destroy(Mmap** me)
{
	for(unsigned int i = 0; i < (*me)->header.count; i++){
		UnmapViewOfFile((*me)->buffers[i]);
	}

	CloseHandle((*me)->file);
	free((*me)->name);
	free(*me);
	*me = NULL;
}

bool Mmap_SendMessage(Mmap* me, const char* type, const char* message, uint32_t length, int timeout)
{
	//FlogD("Sending message");
	char* buffer = Mmap_AcquireBufferW(me, timeout);
	if(buffer){
		memcpy(buffer, message, length);
		Mmap_ReturnBufferW(me, &buffer, length, type);
		//FlogD("Message successfully sent");
		return true;
	}
	//FlogD("Failed to send message");
	return false;
}

uint32_t Mmap_RecvMessage(Mmap* me, char* type, char* message, int timeout)
{
	MmapMessageHeader header;
	const char* buffer = Mmap_AcquireBufferR(me, &header, timeout);
	if(buffer){
		memcpy(message, buffer, header.length);
		strcpy(type, header.type);
		Mmap_ReturnBufferR(me, &buffer); 
		return header.length;
	}
	return -1;
}
