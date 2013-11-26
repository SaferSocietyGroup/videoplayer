#ifndef MMAP_H
#define MMAP_H

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN

#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdbool.h>
#include <stdint.h>

// asserts that a evaluates as true, otherwise exits the application with the error message msg
#define MMAssertMsg(a, ...) if(!(a)){\
	char buffer[512];\
	sprintf(buffer, __VA_ARGS__);\
	FlogF(buffer);\
	exit(1); \
}

#define BENCH_COUNT 2048

#define MMAP_MESSAGE_TYPE_LENGTH 256
#define TEST_MESSAGE "abcdefghjiklmnopqrstuvwxyz0123456789"
#define TEST_MESSAGE_TYPE "test message"

typedef enum {AM_READ, AM_WRITE} AccessMode;

typedef struct __attribute__ ((packed)) {
	uint32_t size;
	uint32_t count;
} MmapHeader;

typedef struct __attribute__ ((packed)) {
	char type[MMAP_MESSAGE_TYPE_LENGTH];
	uint32_t length;
} MmapMessageHeader;

typedef struct
{
	MmapHeader header;

	wchar_t* name;
	int bufferPos;
	char** buffers;
	bool bufferInUse;
	AccessMode mode;
	HANDLE readSem, writeSem;
	HANDLE file;
} Mmap;

Mmap* Mmap_Create(const char* name, uint32_t bufferSize, uint32_t bufferCount, AccessMode mode);
Mmap* Mmap_Open(const char* name, AccessMode mode);

const char* Mmap_AcquireBufferR(Mmap* me, MmapMessageHeader* header, int timeout);
void Mmap_ReturnBufferR(Mmap* me, const char** buffer);

char* Mmap_AcquireBufferW(Mmap* me, int timeout);

void Mmap_ReturnBufferW(Mmap* me, char** buffer, uint32_t length, const char* type);

bool Mmap_SendMessage(Mmap* me, const char* type, const char* message, uint32_t length, int timeout);
uint32_t Mmap_RecvMessage(Mmap* me, char* type, char* message, int timeout);

void Mmap_Destroy(Mmap** me);

double GetTick();

#endif
