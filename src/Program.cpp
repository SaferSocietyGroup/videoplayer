#include <iostream>
#include <cstdint>

#include <SDL.h>

#define WIN32_LEAN_AND_MEAN
#include "windows.h"

#include "Program.h"
#include "ArgParser.h"
#include "Flog.h"
#include "CommandQueue.h"
#include "Tools.h"
#include "Video.h"
#include "FileStream.h"
#include "SdlAudioDevice.h"
#include "Lfscpp.h"

class CProgram : public Program
{
	public:
	std::string pipeName;
	std::string sWindowId;
	LfscppPtr lfs;
	
	VideoPtr video;

	SdlAudioDevicePtr audio;

	bool done = false;
	SDL_Overlay* overlay = 0;

	SDL_Rect rect = {0, 0, 640, 480};

	CommandQueuePtr qCmd;
	SDL_Surface* window;

	Video::ErrorCallback handleError;

	void HandleCommand(Command cmd)
	{
		switch(cmd.type){
			case CTQuit:
				done = true;
				FlogD("quit");
				break;

			case CTPlay:
				audio->SetPaused(false);
				if(video)
					video->play();
				break;

			case CTPause:
				audio->SetPaused(true);
				if(video)
					video->pause();
				break;

			case CTSeek:
				if(video)
					video->seek(cmd.args[0].f);
				break;

			case CTLoad: {
					StreamPtr s;

					if((LoadType)cmd.args[0].i == LTFile){
						// open a file directly
						FileStreamPtr fs = FileStream::Create();
						fs->Open(Tools::WstrToStr(cmd.args[1].str), false);
						s = fs;
					}

					else{
						// open an lfs stream
						s = lfs->Open(cmd.args[1].str);
					}

					video = Video::Create(s, handleError, audio, 64);

					if(overlay)
						SDL_FreeYUVOverlay(overlay);

		 			overlay = SDL_CreateYUVOverlay(640, 480, SDL_YUY2_OVERLAY, window);
				}
				break;

			case CTUnload:
				video = 0;
				audio->SetPaused(true);
				break;

			case CTLfsConnect:
				lfs->Connect(cmd.args[0].str, 1000);
				break;
			
			case CTLfsDisconnect:
				lfs->Disconnect();
				break;

			default:
				throw std::runtime_error(Str("unknown command: " << (int)cmd.type));
				break;
		}
	}
	
	void Interface()
	{
		SDL_Init(SDL_INIT_EVERYTHING);

		window = SDL_SetVideoMode(640, 480, 0, 0);
		FlogAssert(window, "could not set video mode");

		audio = SdlAudioDevice::Create();
		audio->Init(48000, 2, [&](int16_t* data, int nSamples) -> int {
			if(video != 0) 
				return video->fetchAudio(data, nSamples);

			return 0;
		});

		SDL_Event event;
				
		handleError = [&](Video::Error e, const std::string& msg){
		};

		lfs = Lfscpp::Create();

		while(!done){
			uint32_t timer = SDL_GetTicks();

			while(SDL_PollEvent(&event)){
				if(event.type == SDL_QUIT)
					done = true;
			}

			Command cmd;
			while(qCmd->Dequeue(cmd)){
				try {
					HandleCommand(cmd);
				}

				catch (std::runtime_error e) {
					FlogE(Str("failed to handle command: " << e.what()));
				}
			}

			if(video && overlay){
				bool updated = video->update(0);

				if(updated){
					SDL_LockYUVOverlay(overlay);
					video->updateOverlay(overlay->pixels, overlay->pitches, overlay->w, overlay->h);
					SDL_UnlockYUVOverlay(overlay);
					SDL_DisplayYUVOverlay(overlay, &rect);
				}
			}

			timer = SDL_GetTicks() - timer;

			if(timer < 16){
				SDL_Delay(16 - timer);
			}
		}
					
		if(overlay)
			SDL_FreeYUVOverlay(overlay);
	}

	int Run(int argc, char** argv)
	{
		try {
			bool showHelp = argc == 1;

			ArgParserPtr arg = ArgParser::Create();

			arg->AddSwitch('h', "help", "Show help.", [&](){ showHelp = true; });

			arg->AddSwitchArg('p', "pipe-name", "PIPE_NAME", "Specify pipe to use for IPC.", [&](const std::string& arg){ pipeName = arg; });
			arg->AddSwitchArg('w', "window-id", "WINDOW_ID", "Specify window ID to draw onto.", [&](const std::string& arg){ sWindowId = arg; });

			std::vector<std::string> rest = arg->Parse(argc, argv);

			if(sWindowId == "")
				showHelp = true;

			if(showHelp)
			{
				std::cout << "Usage: " << argv[0] << " [OPTION]... [FILE]" << std::endl;
				std::cout << arg->GetHelp();
				return 0;
			}

			FlogExpD(pipeName);
			FlogExpD(sWindowId);
	
			HWND hwnd = (HWND)atol(sWindowId.c_str());
			FlogExpD((intptr_t)hwnd);

			char class_name[512];
			char title[512];

			GetClassName(hwnd, class_name, sizeof(class_name));
			GetWindowText(hwnd, title, sizeof(title));

			FlogExpD(title);
			FlogExpD(class_name);

			char buffer[512];
			sprintf(buffer, "SDL_WINDOWID=%i", (intptr_t)hwnd); 
			SDL_putenv(buffer); 

			qCmd = CommandQueue::Create();
			qCmd->Start(Tools::StrToWstr(pipeName));

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

		SDL_Quit();

		return 0;
	}
};

ProgramPtr Program::Create()
{
	return std::make_shared<CProgram>();
}
