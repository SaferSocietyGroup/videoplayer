#include <SDL.h>
#include "Program.h"

int main(int argc, char** argv)
{
	return Program::Create()->Run(argc, argv);
}
