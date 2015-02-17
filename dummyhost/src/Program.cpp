#include <iostream>
#include <cstdint>
#include <thread>
#include <memory>
#include <mutex>
#include <string>
#include <map>
#include <functional>
#include <condition_variable>

#include <SDL.h>
#include <SDL_syswm.h>

#define WIN32_LEAN_AND_MEAN
#include "windows.h"

#include "Program.h"
#include "ArgParser.h"
#include "Flog.h"
#include "CommandQueue.h"
#include "StringTools.h"
#include "Pipe.h"

#define MAGIC 0xaabbaacc

class CommandLine
{
	public:
	std::shared_ptr<std::thread> thread;
	bool done = false;

	PipePtr pipe;

	void Init(PipePtr pipe)
	{
		this->pipe = pipe;

		thread = std::make_shared<std::thread>([&](){
			std::string line;
		
			std::map<std::string, CommandType> cmdStrs = {
				{"quit", CTQuit},
				{"play", CTPlay},
				{"pause", CTPause},
				{"stop", CTStop},
				{"seek", CTSeek},
				{"load", CTLoad},
				{"unload", CTUnload}
			};

			while(!done){
				std::cout << "> ";
				getline(std::cin, line);
				std::cout << line << std::endl;
				
				try {
					StrVec cmds = StringTools::Split(line, ' ', 1);

					if(cmds.size() == 0)
						throw std::runtime_error("expected command");

					auto it = cmdStrs.find(cmds[0]);
					if(it == cmdStrs.end())
						throw std::runtime_error(Str("no such command: " << cmds[0]));
					
					CommandType cmd = it->second;
					
					if(cmds.size() - 1 != CommandArgs[cmd].size())
						throw std::runtime_error(Str(cmds[0] << " expects " << CommandArgs[cmd].size() << " args (not " << cmds.size() - 1 << ")"));

					if(cmd == CTQuit)
						done = true;

					pipe->WriteUInt32(MAGIC);
					pipe->WriteUInt32((uint32_t)cmd);

					int i = 1;
					for(ArgumentType aType : CommandArgs[cmd]){
						switch(aType){
							case ATStr:    pipe->WriteString(Tools::StrToWstr(cmds[i]));  break;
							case ATInt32:  pipe->WriteInt32(atoi(cmds[i].c_str()));  break;
							case ATFloat:  pipe->WriteFloat(atof(cmds[i].c_str()));  break;
							case ATDouble: pipe->WriteDouble(atof(cmds[i].c_str())); break;
						}

						i++;
					}
					
					pipe->WriteUInt32(MAGIC);
				}

				catch (std::runtime_error e)
				{
					std::cerr << "error: " << e.what() << std::endl;
				}
			}
		});
	}
};

class CProgram : public Program
{
	public:
	std::string pipeName;
	std::string sWindowId;

	SDL_Surface* window;
	
	void Interface()
	{
		window = SDL_SetVideoMode(640, 480, 0, 0);
		FlogAssert(window, "could not set video mode");

		SDL_SysWMinfo info;
		SDL_GetWMInfo(&info);
		std::cout << (intptr_t)info.window << std::endl;

		SDL_Event event;

		CommandLine cli;

		PipePtr pipe = Pipe::Create();
		pipe->CreatePipe(L"videotest");
		cli.Init(pipe);

		SDL_FillRect(window, 0, 0x3366aa);
		SDL_Flip(window);

		while(!cli.done){
			while(SDL_PollEvent(&event)){
				if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
					cli.done = true;
			}

			SDL_Delay(16);
		}
		
		cli.thread->join();
	}

	int Run(int argc, char** argv)
	{
		try {
			bool showHelp = false;

			ArgParserPtr arg = ArgParser::Create();

			arg->AddSwitch('h', "help", "Show help.", [&](){ showHelp = true; });

			std::vector<std::string> rest = arg->Parse(argc, argv);

			if(showHelp)
			{
				std::cout << "Usage: " << argv[0] << " [OPTION]" << std::endl;
				std::cout << arg->GetHelp();
				return 0;
			}

			Interface();
		}

		catch (std::runtime_error ex){
			FlogF(ex.what());
			return 1;
		}

		catch (std::exception ex)
		{
			FlogF(ex.what());
			return 1;
		}

		return 0;
	}
};

ProgramPtr Program::Create()
{
	return std::make_shared<CProgram>();
}

