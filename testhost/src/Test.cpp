#include <SDL.h>
#include <SDL_syswm.h>
#include <sstream>
#include <fstream>

#include "Tools.h"
#include "CommandLine.h"
#include "IpcMessageQueue.h"
#include "Flog.h"

#define CMD_REQ(__cmd, __numargs) \
	if(arg[0] == (__cmd) && arg.size() - 1 != (__numargs)) {\
		 FlogE(__cmd << " requires " << __numargs << " arguments"); \
	} \
	else if(arg[0] == (__cmd)) 

#ifdef _USEWINMAIN
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
int main(int argc, char** argv)
#endif
{
	IpcMessageQueuePtr ipc = IpcMessageQueue::Create("videotest", 4, 65536, 4, 65536);

	SDL_Init(SDL_INIT_EVERYTHING);

	volatile SDL_Surface* screen = SDL_SetVideoMode(800, 600, 0, SDL_SWSURFACE | SDL_RESIZABLE);
	screen->w = screen->w;

	SDL_SysWMinfo info;
	memset(&info, 0, sizeof(SDL_SysWMinfo));
	SDL_VERSION(&info.version);
	if (SDL_GetWMInfo(&info))
		FlogI("window handle: " << (intptr_t)info.window);


	bool done = false;
	int positionCounter = 0;

	CommandLine::Start();

	while(!done){
		SDL_Event event;
		while(SDL_PollEvent(&event)){
			if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE){
				done = true;
			}
		}

		std::vector<std::string> arg;
		if((arg = CommandLine::GetCommand()).size() > 0){
			if(arg[0] == "exit"){
				done = true;
				ipc->WritePacket("quit");
			}

			CMD_REQ("volume", 1) ipc->WritePacket("volume", arg[1]);
			else CMD_REQ("qvmute", 1) ipc->WritePacket("qvmute", arg[1]);
			else CMD_REQ("mute", 1) ipc->WritePacket("mute", arg[1]);
			else CMD_REQ("load", 1) ipc->WritePacket("load", arg[1]);
			else CMD_REQ("play", 0) ipc->WritePacket("play", ""); 
			else CMD_REQ("pause", 0) ipc->WritePacket("pause", ""); 
			else CMD_REQ("getinfo", 0) ipc->WritePacket("getinfo", ""); 
			else CMD_REQ("setdims", 1) ipc->WritePacket("setdims", arg.at(1));
			else CMD_REQ("seek", 1) ipc->WritePacket("seek", arg.at(1));
			else CMD_REQ("step", 1) ipc->WritePacket("step", arg.at(1));
			else CMD_REQ("snapshot", 0) ipc->WritePacket("snapshot", "");
			else CMD_REQ("getkeyframes", 0) ipc->WritePacket("getkeyframes", "");
			else CMD_REQ("setkeyframes", 1) ipc->WritePacket("getkeyframes", arg.at(1));
			else CMD_REQ("setplaybackspeed", 1) ipc->WritePacket("setplaybackspeed", arg.at(1));
			else CMD_REQ("setquickviewplayer", 1) ipc->WritePacket("setquickviewplayer", arg.at(1));
			else CMD_REQ("setactive", 1) ipc->WritePacket("setactive", arg.at(1));

			else CMD_REQ("host_suspend", 1)
				SDL_Delay(atof(arg.at(1).c_str()) * 1000.0f);

			else FlogE("no such command: " << arg[0]);
		}

		std::string type, pkt;
		while(ipc->ReadPacket(type, pkt)){
					if (type == "position"){
						if((positionCounter++) % 10 == 0)
							FlogD("pos: " << pkt);
					}

					else if(type == "snapshot"){
						std::ofstream s("snapshot.raw", std::ios::binary);
						s.write(pkt.c_str(), pkt.size());
						s.close();
					}

					else{
						FlogExpD(type);
						FlogExpD(pkt);
					}
		}

#if 0
		ipc->GetReadBuffer([&](const std::string& type, const char* buffer, size_t size) {
				if(buffer){
					if(type == "frame"){

					/*uint16_t width = *((uint16_t*)buffer.data);
						uint16_t height = *(((uint16_t*)buffer.data) + 1);

						if(!surface || surface->w != (int)width || surface->h != (int)height){
						if(surface) SDL_FreeSurface(surface);

						FlogV("new width, height: " << width << ", " << height);
						surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 24, 0xff0000, 0xff00, 0xff, 0x0);
						}

						memcpy(surface->pixels, buffer.data + 4, width * height * 3);

						if(surface){
						SDL_BlitSurface(surface, NULL, screen, NULL);
						SDL_Flip(screen);
						}*/
					}

					else if(type == "keepalive"){
						ipc->WritePacket("keepalive", "");
						FlogD("keepalive");
					}

					else if (type == "keyframes"){
						std::string str(buffer, size);
						FlogD(str);
					}

					else if (type == "position"){
						if((positionCounter++) % 10 == 0)
							FlogD("pos: " << std::string(buffer, size));
					}

					else if (type == "eof"){
						FlogD("eof");
					}

					else {
						FlogD("got unhandled message type: " << type << " data: " << std::string(buffer, size));
					}
				}
		}, 10);
#endif

		SDL_Delay(1);
	}

	return 0;
}
