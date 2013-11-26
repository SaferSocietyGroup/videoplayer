#include <stdio.h>
#include "mmap.h"
#include "flog.h"

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN

#include <windows.h>

int main(int argc, char** argv)
{
	char type[256];
	char* buffer = malloc(1024 * 1024 * 10);

	if(argc > 1 && !strcmp(argv[1], "host")){
		FlogI("host");
		Mmap* mmap = Mmap_Open("test", AM_WRITE);
		FlogAssert(mmap, "could not create mmap");

		int i = 0;
		while(1){
			FlogD("i: %d", i);
			Mmap_SendMessage(mmap, "test", "test", 6, 1);
		}

		Mmap_Destroy(&mmap);

	}else{

		FlogI("client");
		Mmap* mmapr = Mmap_Create("test", 1024 * 1024 * 10, 32, AM_READ);
		FlogAssert(mmapr, "could not open mmap");

		while(1){
			Sleep(1000);
			if(Mmap_RecvMessage(mmapr, type, buffer, 1000) != -1){
				FlogD("got a message");
			}else{
				FlogD("no message");
			}
		}

		Mmap_Destroy(&mmapr);
	}


	return 0;
}
