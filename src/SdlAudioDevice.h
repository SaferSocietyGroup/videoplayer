#ifndef SDLAUDIODEVICE_H
#define SDLAUDIODEVICE_H

#include "IAudioDevice.h"


typedef std::shared_ptr<class SdlAudioDevice> SdlAudioDevicePtr;

class SdlAudioDevice : public IAudioDevice
{
	public:
	virtual bool Init(int freq, int channels) = 0;
	static SdlAudioDevicePtr Create();
};

#endif
