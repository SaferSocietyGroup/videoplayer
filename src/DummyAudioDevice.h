#ifndef SSGVIDEOPLAYER_DUMMYAUDIODEVICE_H
#define SSGVIDEOPLAYER_DUMMYAUDIODEVICE_H

#include <functional>
#include <cstdint>

#include "IAudioDevice.h"

typedef std::shared_ptr<class DummyAudioDevice> DummyAudioDevicePtr;

class DummyAudioDevice : public IAudioDevice
{
	public:
	static DummyAudioDevicePtr Create();
};

#endif
