#ifndef TIMEPROVIDER
#define TIMEPROVIDER

#include <functional>
#include <memory>

#include "Sample.h"

typedef std::shared_ptr<class IAudioDevice> IAudioDevicePtr;

class IAudioDevice {
	public:
	float volume = 1.0f;

	virtual double GetDeltaTime() = 0;
	virtual int GetRate() = 0;
	virtual int GetChannels() = 0;
	virtual void ClearQueue() = 0;
	virtual void SetPaused(bool paused) = 0;
	virtual void Enqueue(const Sample* buffer, int size) = 0;
};

#endif
