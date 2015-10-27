#ifndef FRAME_H
#define FRAME_H

#include <memory>
#include <vector>
#include "avlibs.h"

typedef std::shared_ptr<class Frame> FramePtr;

struct Sample {
	int16_t chn[2];
	double ts = 0.0f;
	int frameIndex = 0;
};

class Frame
{
	public:
	enum FrameType
	{
		TVideo,
		TAudio,
		TUnknown
	};

	FrameType type = TUnknown;

	virtual AVFrame* GetAvFrame() = 0;
	virtual int64_t GetPts() = 0;
	virtual void SetPts(int64_t pts) = 0;
	virtual FramePtr Clone() = 0;
	virtual void AddSamples(const std::vector<Sample>& samples) = 0;
	virtual const std::vector<Sample>& GetSamples() = 0;
	
	static FramePtr Create(AVFrame* avFrame, uint8_t* buffer, int64_t pts, bool shallowFree);
};

class CompareFrames
{
	public:
	bool operator()(FramePtr a, FramePtr b) const
	{
		return a->GetPts() > b->GetPts();
	}
};

#endif
