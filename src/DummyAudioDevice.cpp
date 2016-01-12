#include <memory>
#include <thread>
#include <chrono>
#include <mutex>
#include <map>

#include "DummyAudioDevice.h"
#include "Flog.h"

class CDummyAudioDevice : public DummyAudioDevice
{
	public:
	int freq = 44100;
	int channels = 2;
	int blockSize = 1024;
	std::function<int(int16_t* data, int nSamples)> update;
	std::shared_ptr<std::thread> t;
	std::mutex mutex;
	std::thread::id noLockForId;

	bool done = false;
	bool paused = false;

	~CDummyAudioDevice()
	{
		done = true;
		t->join();
	}

	void SetPaused(bool paused)
	{
		this->paused = paused;
	}

	bool Init(int freq, int channels, int blockSize, std::function<int(int16_t* data, int nSamples)> update)
	{
		this->freq = freq;
		this->channels = channels;
		this->blockSize = blockSize;
		this->update = update;

		t = std::make_shared<std::thread>([&]()
		{
			noLockForId = std::this_thread::get_id();

			double sleepSeconds = (double)this->blockSize / this->freq;
			int16_t data[blockSize * channels];

			while(!done){
				if(!paused){
					mutex.lock();
					this->update(data, this->blockSize);
					mutex.unlock();
				}

				std::this_thread::sleep_for(std::chrono::duration<double>(sleepSeconds));
			}
		});

		return true;
	}
	
	int GetRate()
	{
		return freq;
	}

	int GetChannels()
	{
		return channels;
	}

	int GetBlockSize()
	{
		return blockSize;
	}
	
	void Lock(bool value)
	{
		if(std::this_thread::get_id() == noLockForId)
			return;

		if(value)
			mutex.lock();
		else
			mutex.unlock();
	}
};

DummyAudioDevicePtr DummyAudioDevice::Create()
{
	return std::make_shared<CDummyAudioDevice>();
}
