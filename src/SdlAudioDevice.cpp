#include <SDL.h>
#include <queue>

#include "SdlAudioDevice.h"
#include "Flog.h"

class CSdlAudioDevice : public SdlAudioDevice
{
	public:
	SDL_AudioSpec spec;
	std::function<int(int16_t* data, int nSamples)> update;
	
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
		int nSmp = len / 2 / spec.channels;
		int16_t* data = (int16_t*)stream;

		int fetched = update(data, nSmp);

		for(int i = fetched; i < nSmp; i++){
			data[i*2+0] = 0;
			data[i*2+1] = 0;
		}
	}
	
	bool Init(int freq, int channels, int blockSize, std::function<int(int16_t* data, int nSamples)> update)
	{
		this->update = update;

		SDL_AudioSpec fmt;

		memset(&fmt, 0, sizeof(SDL_AudioSpec));
		memset(&spec, 0, sizeof(SDL_AudioSpec));

		fmt.freq = freq;
		fmt.format = AUDIO_S16LSB;
		fmt.channels = channels;
		fmt.samples = blockSize;
		fmt.callback = SdlCallback;
		fmt.userdata = this;

		if(SDL_OpenAudio(&fmt, &spec) != 0)
			return false;

		FlogExpD(spec.freq);
		FlogExpD((int)spec.channels);
		FlogExpD(spec.samples);

		return true;
	}
	
	int GetRate()
	{
		return spec.freq;
	}

	int GetChannels()
	{
		return spec.channels;
	}

	int GetBlockSize()
	{
		return spec.samples;
	}
	
	void Lock(bool value)
	{
		if(value)
			SDL_LockAudio();
		else
			SDL_UnlockAudio();
	}
};

SdlAudioDevicePtr SdlAudioDevice::Create()
{
	return std::make_shared<CSdlAudioDevice>();
}
