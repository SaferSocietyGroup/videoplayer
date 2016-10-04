#ifndef TIMEPROVIDER
#define TIMEPROVIDER

#include <functional>
#include <memory>

typedef std::shared_ptr<class IAudioDevice> IAudioDevicePtr;

class IAudioDevice {
	public:
	virtual bool Init(int freq, int channels, int blockSize, std::function<int(int16_t* data, int nSamples)> update) = 0;
	virtual int GetRate() = 0;
	virtual int GetBlockSize() = 0;
	virtual int GetChannels() = 0;
	virtual void SetPaused(bool paused) = 0;
	virtual void Lock(bool value) = 0;
};

#endif
