/*
 * NetClean VideoPlayer
 *  Multi process video player for windows.
 *
 * Copyright (c) 2010-2013 NetClean Technologies AB
 * All Rights Reserved.
 *
 * This file is part of NetClean VideoPlayer.
 *
 * NetClean VideoPlayer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * NetClean VideoPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NetClean VideoPlayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DEBUG
#include "log.h"
#include "audiohandler.h"

#include <algorithm>
#include <SDL.h>

#include "avlibs.h"

#include "timehandler.h"
#include "iaudiodevice.h"

#include <queue>
#include <functional>

class CAudioHandler : public AudioHandler
{
	std::function<void(const Sample* buffer, int size)> audioCb;
	IAudioDevice* audioDevice;
	int channels;
	int freq;
	float lastTimeWarp = 0;

	AVCodecContext *aCodecCtx;
	AVCodec *aCodec;
	SwrContext* swr;
		
	uint8_t** dstBuf;
	int dstLineSize;
	
	double timeFromPts(int64_t pts, AVRational timeBase){
		return (double)pts * av_q2d(timeBase);
	}

	public:
	CAudioHandler(AVCodecContext* aCodecCtx, IAudioDevice* audioDevice) : audioDevice(audioDevice)
	{
		this->aCodecCtx = aCodecCtx;

		this->channels = audioDevice->getChannelCount();
		this->freq = audioDevice->getSampleRate();
	
		aCodec = avcodec_find_decoder(aCodecCtx->codec_id);

		if(!aCodec || avcodec_open2(aCodecCtx, aCodec, NULL) < 0)
		{
			LogError("unsupported audio codec");
			return;
		}

		this->swr = 0;
		this->dstBuf = 0;
	}

	~CAudioHandler()
	{
		if(aCodecCtx){
			avcodec_close(aCodecCtx);
		}

		if(dstBuf)
			av_freep(&dstBuf[0]);
		av_freep(&dstBuf);

		if(this->swr)
			swr_free(&this->swr);
	}

	int decode(AVPacket& packet, AVStream* stream, double timeWarp){
		if(!aCodec)
			return -1;

		AVFrame* frame = av_frame_alloc();
		FlogAssert(frame, "could not allocate frame");

		int got_frame = 0;
		int bytesDecoded = avcodec_decode_audio4(aCodecCtx, frame, &got_frame, &packet);

		if(bytesDecoded >= 0 && got_frame){
			if(!this->swr || (lastTimeWarp != timeWarp)){
				lastTimeWarp = timeWarp;

				if(this->swr)
					swr_free(&this->swr);

				int64_t chLayout = frame->channel_layout != 0 ? frame->channel_layout : 
					av_get_default_channel_layout(frame->channels);

				this->swr = swr_alloc_set_opts(NULL, av_get_default_channel_layout(this->channels), 
					AV_SAMPLE_FMT_S16, this->freq / timeWarp, chLayout, (AVSampleFormat)frame->format, 
					(float)frame->sample_rate, 0, NULL);
				
				FlogAssert(this->swr, "error allocating swr");
				
				swr_init(this->swr);

				int ret = av_samples_alloc_array_and_samples(&dstBuf, &dstLineSize, 2, 
					this->freq * this->channels, AV_SAMPLE_FMT_S16, 0);

				FlogAssert(ret >= 0, "error allocating samples: " << ret);
			}
				
			int64_t pts = av_frame_get_best_effort_timestamp(frame);
			double ts = timeFromPts(pts, stream->time_base);

			int dstSampleCount = av_rescale_rnd(frame->nb_samples, freq / timeWarp, aCodecCtx->sample_rate, AV_ROUND_UP);

			int ret = swr_convert(swr, dstBuf, dstSampleCount, (const uint8_t**)frame->data, frame->nb_samples);

			if(ret >= 0){
				auto asmp = std::auto_ptr<Sample>(new Sample [dstSampleCount]);
				Sample* smp = asmp.get();

				for(int i = 0; i < dstSampleCount; i++){
					smp[i].chn[0] = ((int16_t*)dstBuf[0])[i * 2];
					smp[i].chn[1] = ((int16_t*)dstBuf[0])[i * 2 + 1];
					smp[i].ts = ts;
				}

				audioDevice->EnqueueSamples(smp, dstSampleCount);
			}
		}

		av_frame_free(&frame);

		return bytesDecoded;
	}

	int getSampleRate(){
		if(aCodecCtx){
			return aCodecCtx->sample_rate;
		}
		return 0;
	}

	int getChannels(){
		if(aCodecCtx){
			return aCodecCtx->channels;
		}
		return 0;
	}

	int getBitRate(){
		if(aCodec){
			return aCodecCtx->bit_rate;
		}
		return 0;
	}

	const char* getCodec(){
		if(aCodec){
			return aCodec->name;
		}
		return "none";
	}
};

AudioHandlerPtr AudioHandler::Create(AVCodecContext* aCodecCtx, IAudioDevice* audioDevice)
{
	return AudioHandlerPtr(new CAudioHandler(aCodecCtx, audioDevice));
}
