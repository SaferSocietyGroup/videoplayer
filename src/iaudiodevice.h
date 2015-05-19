#ifndef IAUDIODEVICE_H
#define IAUDIODEVICE_H

#include "samplequeue.h"

class IAudioDevice
{
	public:
	virtual void emptyQueue() = 0;
	virtual int getBlockSize() = 0;
	virtual int getSampleCount() = 0;
	virtual int getSampleRate() = 0;
	virtual int getChannelCount() = 0;
	virtual void EnqueueSamples(const Sample* buffer, int size) = 0;
};

#endif
