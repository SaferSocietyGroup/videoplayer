#include "commandline.h"
#include "log.h"
#include <iostream>
#include <sstream>

std::queue<std::vector<std::string> > CommandLine::queue;
SDL_mutex* CommandLine::mutex = 0;
SDL_Thread* CommandLine::thread = 0;
	
void CommandLine::Start()
{
	mutex = SDL_CreateMutex();	
	thread = SDL_CreateThread(ThreadCallback, NULL);
}

int CommandLine::ThreadCallback(void* data)
{
	std::string line;
	bool done = false;

	while(!done){
		std::vector<std::string> cmd;
		
		std::getline(std::cin, line);
		std::stringstream parse(line);
		std::string tmp;

		parse >> tmp;
		cmd.push_back(tmp);

		while(parse.peek() == ' ')
			parse.get();

		std::getline(parse, tmp);
		if(tmp != "")
			cmd.push_back(tmp);

		/*while(parse.good()){
			std::string tmp;
			parse >> tmp;
			cmd.push_back(tmp);
		}*/

		SDL_mutexP(mutex);
		queue.push(cmd);
		SDL_mutexV(mutex);
		SDL_Delay(1);

		if((std::string)line == "exit" || !std::cin.good()){
			done = true;
		}
	}

	std::vector<std::string> exit = {"exit"};

	queue.push(exit);
	FlogI("exiting");

	return 1;
}

void CommandLine::AddCommand(std::vector<std::string> command)
{
	SDL_mutexP(mutex);
	queue.push(command);
	SDL_mutexV(mutex);
}

std::vector<std::string> CommandLine::GetCommand()
{
	std::vector<std::string> ret;

	SDL_mutexP(mutex);
	if(queue.size()){
		ret = queue.front();
		queue.pop();
	}
	SDL_mutexV(mutex);

	return ret;
}

