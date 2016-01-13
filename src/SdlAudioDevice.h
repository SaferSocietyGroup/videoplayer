#ifndef SDLAUDIODEVICE_H
#define SDLAUDIODEVICE_H

#include <functional>
#include <cstdint>

#include "IAudioDevice.h"

typedef std::shared_ptr<class SdlAudioDevice> SdlAudioDevicePtr;

class SdlAudioDevice : public IAudioDevice
{
	public:
	static SdlAudioDevicePtr Create();
};

#endif
