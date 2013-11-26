#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include <SDL.h>

#include <queue>
#include <string>
#include <vector>

class CommandLine
{
	public:
	static void Start();
	static std::vector<std::string> GetCommand();
	static void AddCommand(std::vector<std::string> command);

	private:
	static int ThreadCallback(void* data);
	static std::queue<std::vector<std::string> > queue;
	static SDL_mutex* mutex;
	static SDL_Thread *thread;
};

#endif

