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
#include "CommandSender.h"
#include "StringTools.h"
#include "Pipe.h"

#define MAGIC 0xaabbaacc

class CommandLine
{
	public:
	std::shared_ptr<std::thread> thread;
	std::shared_ptr<std::thread> recvThread;
	bool done = false;
	bool showMessages = false;

	CommandSenderPtr cmdSend;
	CommandQueuePtr cmdRecv;

	void RecvThread()
	{
		while(!done){
			Command cmd;
			if(cmdRecv->Dequeue(cmd)){
				if(showMessages){
					if(cmd.type == CTPositionUpdate){
						FlogD("position update: " << cmd.args[0].f);
					}else if(cmd.type == CTDuration){
						FlogD("duration: " << cmd.args[0].f);
					}else{
						FlogD("unknown command from player");
					}
				}
			}

			else{
				SDL_Delay(100);
			}
		}
	}

	void Init(CommandSenderPtr cmdSend, CommandQueuePtr cmdRecv)
	{
		this->cmdSend = cmdSend;
		this->cmdRecv = cmdRecv;

		recvThread = std::make_shared<std::thread>([&](){ RecvThread(); });

		thread = std::make_shared<std::thread>([&](){
			std::string line;
		
			std::map<std::string, CommandType> cmdStrs = {
				{"quit", CTQuit},
				{"play", CTPlay},
				{"pause", CTPause},
				{"stop", CTStop},
				{"seek", CTSeek},
				{"load", CTLoad},
				{"unload", CTUnload},
				{"update-output-size", CTUpdateOutputSize},
			};

			while(!done){
				std::cout << "> ";
				getline(std::cin, line);
				std::cout << line << std::endl;
				
				try {
					StrVec cmds = StringTools::Split(line, ' ');

					if(cmds.size() == 0)
						throw std::runtime_error("expected command");

					if(cmds[0] == "show-messages"){
						if(cmds.size() != 2)
							throw std::runtime_error("command expects true/false");

						showMessages = cmds[1] == "true";
					}

					if(cmds[0] == "seek-through"){
						std::vector<float> positions = {1.0f, 3.0f, 10.0f, 20.0f, 23.0f, 23.5f, 30.0f, 70.0f};
						for(auto pos : positions){
							cmdSend->SendCommand(CTSeek, pos);
							SDL_Delay(500);
						}
					}

					else {
						auto it = cmdStrs.find(cmds[0]);
						if(it == cmdStrs.end())
							throw std::runtime_error(Str("no such command: " << cmds[0]));
						
						Command cmd;
						cmd.type = it->second;
						
						if(cmds.size() - 1 != CommandArgs[cmd.type].size())
							throw std::runtime_error(Str(cmds[0] << " expects " << CommandArgs[cmd.type].size() << " args (not " << cmds.size() - 1 << ")"));

						if(cmd.type == CTQuit)
							done = true;

						int i = 1;

						for(ArgumentType aType : CommandArgs[cmd.type]){
							Argument arg;
							arg.type = aType;

							switch(aType){
								case ATStr:    arg.str = Tools::StrToWstr(cmds[i]);  break;
								case ATInt32:  arg.i = atoi(cmds[i].c_str());        break;
								case ATFloat:  arg.f = atof(cmds[i].c_str());        break;
								case ATDouble: arg.d = atof(cmds[i].c_str());        break;
							}

							cmd.args.push_back(arg);

							i++;
						}

						cmdSend->SendCommand(cmd);
					}
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
	std::wstring pipeName = L"videotest";

	SDL_Surface* window;
	
	void Interface()
	{
		SDL_Init(SDL_INIT_EVERYTHING);

		window = SDL_SetVideoMode(640, 480, 0, 0);
		FlogAssert(window, "could not set video mode");

		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		SDL_GetWMInfo(&info);
		std::cout << "window id: " << (intptr_t)info.window << std::endl;

		SDL_Event event;

		CommandLine cli;

		PipePtr sendPipe = Pipe::Create();
		sendPipe->CreatePipe(pipeName);
		
		PipePtr recvPipe = Pipe::Create();
		recvPipe->CreatePipe(LStr(pipeName << L"_r"));
		
		FlogD("waiting for connection");
		sendPipe->WaitForConnection(-1);
		recvPipe->WaitForConnection(-1);
		FlogD("connected");
		
		CommandSenderPtr cmdSend = CommandSender::Create();
		cmdSend->Start(sendPipe);
		
		CommandQueuePtr cmdRecv = CommandQueue::Create();
		cmdRecv->Start(recvPipe);
		
		cli.Init(cmdSend, cmdRecv);

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
		cli.recvThread->join();

		cmdSend->Stop();
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

