#include <SDL.h>
#include "flog.h"
#include "video.h"

class MessageHandler : public VideoMsgHandler {
	public:
	bool isEof;
	MessageHandler(){
		isEof = false;
	}

	void OnEof() { 
		FlogD("eof");
		isEof = true;
	};

	void OnError(std::string message) { FlogE(message); }
};

class AudioRecv : public AudioBufferRecv {
	public:
	void RecvAudio(const Sample* buffer, int size){
	}
};

int main(int argc, char** argv)
{
	FlogAssert(argc == 2, "usage: " << argv[0] << " [filename]"); 
	Video::initialize();

	MessageHandler mh;
	AudioRecv ar;

	Video* video = Video::CreateVideo(argv[1], &mh, &ar, 48000, 2);

	if(!video){
		return 1;
	}

	video->play();

	uint8_t* buffer = new uint8_t [ 1024 * 1024 * 10 ];

	int i = 0;
	while(!mh.isEof){
		//FlogD(i);
		video->tick();
		video->adjustTime();
		video->timeHandler.addTime(.1);

		Frame frame = video->fetchFrame();

		if(frame.avFrame){
			i++;
			video->frameToSurface(frame, buffer);
			FlogD(i << " " << video->getPosition() 
				<< "/" << video->getDurationInFrames());
		}
	}

	delete video;

	return 0;
}
