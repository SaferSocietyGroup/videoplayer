#include <algorithm>

#include "Frame.h"
#include "Flog.h"

class CFrame : public Frame
{
	public:
	AVFrame* avFrame;
	uint8_t* buffer;
	int64_t pts = AV_NOPTS_VALUE;
	bool shallowFree;
	std::vector<Sample> samples;

	CFrame(AVFrame* avFrame, uint8_t* buffer, int64_t pts, bool shallowFree) 
		: avFrame(avFrame), buffer(buffer), pts(pts), shallowFree(shallowFree)
	{
	}
	
	const std::vector<Sample>& GetSamples()
	{
		return samples;
	}

	void AddSamples(const std::vector<Sample>& newSamples)
	{
		samples.reserve(samples.size() + newSamples.size());
		samples.insert(samples.end(), newSamples.begin(), newSamples.end());
	}
	
	AVFrame* GetAvFrame()
	{
		return avFrame;
	}
	
	int64_t GetPts()
	{
		return pts;
	}
	
	void SetPts(int64_t pts)
	{
		this->pts = pts;
	}
	
	FramePtr Clone()
	{
		AVFrame* src = this->avFrame;
		AVFrame* avFrame = av_frame_alloc();
		uint8_t *buffer = (uint8_t *)av_malloc(avpicture_get_size((AVPixelFormat)src->format, src->width, src->height));

		if(!buffer || !avFrame){
			if(avFrame)
				av_frame_free(&avFrame);

			if(buffer)
				av_free(buffer);

			throw std::runtime_error("allocation failed in cloneframe");
		}

		avpicture_fill((AVPicture *) avFrame, buffer, (AVPixelFormat)src->format, src->width, src->height);
		av_picture_copy((AVPicture*)avFrame, (AVPicture*)src, (AVPixelFormat)src->format, src->width, src->height);

		avFrame->width = src->width;
		avFrame->height = src->height;
		avFrame->format = src->format;

		return Frame::Create(avFrame, buffer, pts, false);
	}
	
	~CFrame()
	{
		if(avFrame != 0){
			if(shallowFree){
				av_free(avFrame);
			}else{
				av_frame_free(&avFrame);
			}
		}

		if(buffer != 0)
			av_free(buffer);
	}
	
	void CopyScaled(AVPicture* target, int w, int h, AVPixelFormat fmt)
	{
		if(avFrame == 0){
			throw std::runtime_error("Frame::CopyScale() called but avFrame is NULL");
		}

		struct SwsContext* swsCtx = sws_getContext(avFrame->width, avFrame->height, 
			(AVPixelFormat)avFrame->format, w, h, fmt, SWS_BILINEAR, NULL, NULL, NULL);
		
		if(swsCtx == 0){
			throw std::runtime_error("Failed to get a scaling context");
		}
		
		sws_scale(swsCtx, (uint8_t**)avFrame->data, avFrame->linesize, 0, 
				avFrame->height, target->data, target->linesize); 

		sws_freeContext(swsCtx);
	}
};

FramePtr Frame::Create(AVFrame* avFrame, uint8_t* buffer, int64_t pts, bool shallowFree)
{
	return std::make_shared<CFrame>(avFrame, buffer, pts, shallowFree);
}
	
FramePtr Frame::CreateEmpty()
{
	return std::make_shared<CFrame>(avcodec_alloc_frame(), (uint8_t*)0, 0, true);
}
