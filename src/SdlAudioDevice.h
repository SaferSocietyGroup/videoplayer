#ifndef SDLAUDIODEVICE_H
#define SDLAUDIODEVICE_H

#include <functional>
#include <cstdint>

#include "IAudioDevice.h"

typedef std::shared_ptr<class SdlAudioDevice> SdlAudioDevicePtr;

class SdlAudioDevice : public IAudioDevice
{
	public:
	virtual bool Init(int freq, int channels, std::function<int(int16_t* data, int nSamples)> update) = 0;
	static SdlAudioDevicePtr Create();
};

#endif
