#include <iostream>
#include <cstdint>
#include <cstdio>

#include <SDL.h>

#define WIN32_LEAN_AND_MEAN
#include "windows.h"

#include "Program.h"
#include "ArgParser.h"
#include "Flog.h"
#include "CommandQueue.h"
#include "CommandSender.h"
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
	int w = 640, h = 480;
	
	bool redraw = false;

	CommandSenderPtr cmdSend;
	CommandQueuePtr qCmd;
	SDL_Surface* window;

	Video::MessageCallback handleMessage;

	void UpdateOutputSize(int w, int h)
	{
		this->w = w;
		this->h = h;

		if(w > window->w || h > window->h){
			SDL_FreeSurface(window);
			window = SDL_SetVideoMode(w, h, 0, 0);
			FlogD("setting new window size: " << w << " x " << h);

			if(!window)
				throw std::runtime_error("could not set new window size");
		}

		redraw = true;

		if(!overlay)
			return;

		float wAspect = (float)w / (float)h;
		float aspect = (float)overlay->w / (float)overlay->h;

		if(wAspect >= aspect){
			rect.w = h * aspect; 
			rect.h = h;
			rect.x = (w - rect.w) / 2;
			rect.y = 0;
		}else{
			rect.w = w;
			rect.h = w / aspect;
			rect.x = 0;
			rect.y = (h - rect.h) / 2;
		}

		// SDL bug? If the overlay is exactly 320 x 240 and the output is exactly 640 x 480, the output is garbled.
		if(overlay->w == 320 && rect.w == 640 && overlay->h == 240 && rect.h == 480){
			rect.w++;
		}

		FlogD("new output size: " << rect.x << ", " << rect.y << ", " << rect.w << ", " << rect.h);
	}

	void HandleCommand(Command cmd)
	{
		FlogExpD(cmd.type);

		switch(cmd.type){
			case CTQuit:
				done = true;
				FlogD("quit");
				break;

			case CTPlay:
				if(video)
					video->play();
				break;

			case CTPause:
				if(video)
					video->pause();
				break;

			case CTSeek:
				if(video){
					try
					{
						video->seek(cmd.args[0].f);
					}

					catch(VideoException e)
					{
						FlogE(e.what());
					}
				}
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

					try {
						video = Video::Create(s, handleMessage, audio);
					}

					catch(VideoException e)
					{
						FlogE("couldn't open video: " << e.what());
						video = 0;
					}

					if(video != 0){
						cmdSend->SendCommand(NO_SEQ_NUM, 0, CTDuration, video->getDuration());
					}

					if(overlay)
						SDL_FreeYUVOverlay(overlay);

					FlogD("creating new overlay: " << video->getWidth() << " x " << video->getHeight());

		 			overlay = SDL_CreateYUVOverlay(video->getWidth(), video->getHeight(), SDL_YUY2_OVERLAY, window);

					if(!overlay)
						throw std::runtime_error("could not create overlay for video");

					UpdateOutputSize(w, h);
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

			case CTUpdateOutputSize:
				UpdateOutputSize(cmd.args[0].i, cmd.args[1].i);
				break;

			case CTForceRedraw:
				redraw = true;
				break;
			
			case CTSetPlaybackSpeed:
				if(video)
					video->setPlaybackSpeed(cmd.args[0].f);
				break;

			case CTSetVolume:
				if(video)
					video->SetVolume(cmd.args[0].f);
				break;

			case CTSetMute:
				if(video)
					video->SetMute(cmd.args[0].i != 0);
				break;
			
			case CTSetQvMute:
				if(video)
					video->SetQvMute(cmd.args[0].i != 0);
				break;

			default:
				throw std::runtime_error(Str("unknown command: " << (int)cmd.type));
				break;
		}
	}
	
	void Interface()
	{
		SDL_Init(SDL_INIT_EVERYTHING);

		window = SDL_SetVideoMode(w, h, 0, 0);
		FlogAssert(window, "could not set video mode");

		audio = SdlAudioDevice::Create();
		audio->Init(48000, 2, [&](int16_t* data, int nSamples) -> int {
			if(video != 0) 
				return video->fetchAudio(data, nSamples);

			return 0;
		});

		SDL_Event event;
				
		handleMessage = [&](Video::MessageType type, const std::string& msg){
			if(type == Video::MEof){
				cmdSend->SendCommand(NO_SEQ_NUM, 0, CTEof);
			}
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
				try {
					bool updated = video->update();

					if(updated){
						SDL_LockYUVOverlay(overlay);
						video->updateOverlay(overlay->pixels, overlay->pitches, overlay->w, overlay->h);
						SDL_UnlockYUVOverlay(overlay);
						redraw = true;

						cmdSend->SendCommand(NO_SEQ_NUM, 0, CTPositionUpdate, video->getPosition());
					}
				}

				catch(VideoException e)
				{
					FlogE(e.what());
				}
			}
					
			if(redraw && overlay){
				SDL_DisplayYUVOverlay(overlay, &rect);
				redraw = false;
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
		
			PipePtr cqPipe = Pipe::Create();
			cqPipe->Open(Tools::StrToWstr(pipeName));

			PipePtr csPipe = Pipe::Create();
			csPipe->Open(LStr(Tools::StrToWstr(pipeName) << "_r"));

			qCmd = CommandQueue::Create();
			qCmd->Start(cqPipe);

			cmdSend = CommandSender::Create();
			cmdSend->Start(csPipe);

			Flog_SetCallback([&](Flog_Severity severity, int lineNumber, const char* file, const char* message){
				std::wstring wmessage = Tools::StrToWstr(std::string(message));
				std::wstring wfile = Tools::StrToWstr(std::string(file));
				if(cmdSend != 0){
					cmdSend->SendCommand(NO_SEQ_NUM, 0, CTLogMessage, (int)severity, lineNumber, wfile.c_str(), wmessage.c_str());
				}
			});

			Interface();

			Flog_SetCallback(0);

			cmdSend->Stop();
		}

		catch (std::runtime_error ex){
			FlogF(ex.what());
			return 1;
		}

		catch (VideoException ex)
		{
			FlogF(ex.what());
			return 1;
		}

		catch (std::exception ex)
		{
			FlogF(ex.what());
			return 1;
		}

		if(lfs != 0){
			lfs->Disconnect();
		}

		SDL_Quit();

		return 0;
	}
};

ProgramPtr Program::Create()
{
	return std::make_shared<CProgram>();
}
