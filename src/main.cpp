#define WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include <windows.h>
#include <SDL.h>
#include "Program.h"

int main(int argc, char** argv)
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	// Suppress the abort message
	_set_abort_behavior(0, _WRITE_ABORT_MSG);

	return Program::Create()->Run(argc, argv);
}
