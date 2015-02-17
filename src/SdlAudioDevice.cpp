#include <SDL.h>
#include <queue>

#include "SdlAudioDevice.h"
#include "Flog.h"

class CSdlAudioDevice : public SdlAudioDevice
{
	public:
	std::queue<Sample> queue;
	SDL_AudioSpec spec;
	double dt = 0.0;
	
	void SetPaused(bool paused)
	{
		SDL_PauseAudio(paused);
	}

	static void SdlCallback(void *vMe, Uint8 *stream, int len)
	{
		((CSdlAudioDevice*)vMe)->FillBufferCallback(stream, len);
	}

	void FillBufferCallback(Uint8 *stream, int len)
	{
		for(int i = 0; i < len; i += 4){
			if(!queue.empty()){
				Sample s = queue.front();
				queue.pop();

				for(int j = 0; j < 2; j++){
					s.chn[j] = (int16_t)((float)s.chn[j] * volume);
				}

				stream[i+0] = s.chn[0] & 0xff;
				stream[i+1] = (s.chn[0] >> 8) & 0xff;

				stream[i+2] = s.chn[1] & 0xff;
				stream[i+3] = (s.chn[1] >> 8) & 0xff;
			}else{
				stream[i] = stream[i+1] = stream[i+2] = stream[i+3] = 0;
			}
		}
		
		dt += (double)(len / (int)spec.channels / 2) * 1.0 / (double)spec.freq;
	}

	bool Init(int freq, int channels)
	{
		SDL_AudioSpec fmt;

		memset(&fmt, 0, sizeof(SDL_AudioSpec));
		memset(&spec, 0, sizeof(SDL_AudioSpec));

		fmt.freq = freq;
		fmt.format = AUDIO_S16LSB;
		fmt.channels = channels;
		fmt.samples = 1024;
		fmt.callback = SdlCallback;
		fmt.userdata = this;

		if(SDL_OpenAudio(&fmt, &spec) != 0)
			return false;

		FlogExpD(spec.freq);
		FlogExpD((int)spec.channels);
		FlogExpD(spec.samples);

		return true;
	}
	
	double GetDeltaTime()
	{
		SDL_LockAudio();
		double ret = dt;
		dt = 0.0;
		SDL_UnlockAudio();
		return ret;
	}
	
	int GetRate()
	{
		return spec.freq;
	}

	int GetChannels()
	{
		return spec.channels;
	}

	void ClearQueue()
	{
		SDL_LockAudio();

		while(!queue.empty())
			queue.pop();

		SDL_UnlockAudio();
	}

	void Enqueue(const Sample* buffer, int size)
	{
		SDL_LockAudio();
		for(int i = 0; i < size; i++)
			queue.push(buffer[i]);
		SDL_UnlockAudio();
	}
};

SdlAudioDevicePtr SdlAudioDevice::Create()
{
	return std::make_shared<CSdlAudioDevice>();
}
