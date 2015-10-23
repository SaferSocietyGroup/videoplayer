#ifndef FRAME_H
#define FRAME_H

#include <memory>
#include "avlibs.h"

typedef std::shared_ptr<class Frame> FramePtr;

class Frame
{
	public:
	virtual AVFrame* GetAvFrame() = 0;
	virtual int64_t GetPts() = 0;
	virtual void SetPts(int64_t pts) = 0;
	virtual FramePtr Clone() = 0;
	
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
