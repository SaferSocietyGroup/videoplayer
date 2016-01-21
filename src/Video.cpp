/*
 * SSG VideoPlayer
 *  Multi process video player for windows.
 *
 * Copyright (c) 2010-2015 Safer Society Group Sweden AB
 * All Rights Reserved.
 *
 * This file is part of SSG VideoPlayer.
 *
 * SSG VideoPlayer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * SSG VideoPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SSG VideoPlayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DEBUG

#include <cstdlib>
#include <cassert>
#include <cstring>
#include <fstream>
#include <sstream>
#include <queue>
#include <deque>

#include <algorithm>
#include <stdexcept>
#include <iomanip>
#include <map>

#include "Video.h"
#include "Flog.h"
#include "AudioHandler.h"
#include "AudioHandlerNoSound.h"
#include "TimeHandler.h"
#include "PriorityQueue.h"
#include "Frame.h"
#include "Packet.h"
#include "Tools.h"

typedef std::map<int, FramePtr> StreamFrameMap;

class CVideo : public Video
{
	public:
	MessageCallback messageCallback;
	bool stepIntoQueue = true;

	int w, h;
	int backSeek;
	
	IAudioDevicePtr audioDevice;
	AudioHandlerPtr audioHandler;
	TimeHandlerPtr timeHandler;

	int audioStream = 0;
	int videoStream = 0;
	unsigned maxRetries = 100;
	std::vector<std::string> retryStack;

	AVFormatContext* pFormatCtx = 0;
	AVCodecContext* pCodecCtx = 0;
	AVCodec *pCodec = 0;
	
	static bool drm;
	
	int maxFrameQueueSize = 0;
	int minFrameQueueSize = 16;
	int targetFrameQueueSize = 16;

	PriorityQueue<FramePtr, CompareFrames> frameQueue;
	
	double lastFrameQueuePts = .0;

	FramePtr currentFrame = 0;
	bool reportedEof = false;
	StreamPtr stream;

	int64_t firstDts = AV_NOPTS_VALUE;
	int64_t firstPts = AV_NOPTS_VALUE;
	
	CVideo(MessageCallback messageCallback){
		this->messageCallback = messageCallback;
		this->maxFrameQueueSize = maxFrameQueueSize;
	}

	~CVideo(){
		closeFile();
		emptyFrameQueue();
	}

	FramePtr fetchFrame()
	{
		bool wasStepIntoQueue = stepIntoQueue;

		if(frameQueue.empty() && IsEof() && !reportedEof)
		{
			reportedEof = true;
			messageCallback(MEof, "eof");
		}

		if(stepIntoQueue && !frameQueue.empty())
		{
			stepIntoQueue = false;
			timeHandler->SetTime(timeFromTs(frameQueue.top()->GetPts()) + .001);
			audioHandler->discardQueueUntilTs(timeHandler->GetTime());
		}

		double time = timeHandler->GetTime();

		FramePtr newFrame = 0;

		// Throw away all old frames (timestamp older than now) except for the last
		// and set the pFrame pointer to that.
		int poppedFrames = 0;

		while(!frameQueue.empty() && timeFromTs(frameQueue.top()->GetPts()) < time)
		{
			newFrame = frameQueue.top();
			frameQueue.pop();
			poppedFrames++;
		}
			
		if(poppedFrames > 1){
			FlogD("skipped " << poppedFrames - 1 << " frames");
		}

		if(newFrame != 0 && (newFrame->GetPts() >= time || wasStepIntoQueue))
			return newFrame;

		return 0;
	}

	// some decoders (eg. wmv) return invalid timestamps that are not
	// AV_NOPTS_VALUE. This function detects those values.
	inline bool isValidTs(int64_t ts)
	{
		if(ts == AV_NOPTS_VALUE)
			return false;

		// wmv decoder reports 7ffeffffffffffff, fail on very high values
		if(ts > 0x7f00000000000000LL)
			return false;

		return true;
	}

	void adjustTime(){
		// Checks so that the current time doesn't diff too much from the decoded frames.
		// If it does the stream probably jumped ahead or back, so current time needs to 
		// be adjusted accordingly.

		if(frameQueue.empty())
			return;

		double time = timeHandler->GetTime();
		double pts = timeFromTs(frameQueue.top()->GetPts());

		// If the next frame is far into the future or the past, 
		// set the time to now

		if(pts > time + 100.0 || pts < time - 100.0){
			FlogD("adjusted time");
			timeHandler->SetTime(pts);
		}
	}
	
	int fetchAudio(int16_t* data, int nSamples)
	{
		return audioHandler->fetchAudio(data, nSamples);
	}

	bool update()
	{
		tick();
		adjustTime();
		FramePtr newFrame = fetchFrame();

		if(newFrame != 0){
			// Don't free currentFrame if it is currentFrame itself that's being converted
			if(currentFrame == 0 || currentFrame->GetAvFrame() != newFrame->GetAvFrame()){
				currentFrame = newFrame; // Save the current frame for snapshots etc.
			}

			lastFrameQueuePts = timeFromTs(newFrame->GetPts());
			return true;
		}

		return false;
	}

	void updateOverlay(uint8_t** pixels, const uint16_t* pitches, int w, int h)
	{
		if(currentFrame == 0){
			FlogE("Video::updateOverlay() called but currentFrame is unset");
			throw VideoException(VideoException::EScaling);
		}

		AVPicture pict;
		int avret = avpicture_fill(&pict, NULL, AV_PIX_FMT_YUYV422, w, h);

		if(avret < 0){
			FlogE("avpicture_fill returned " << avret);
			throw VideoException(VideoException::EScaling);
		}

		for(int i = 0; i < 3; i++){
			pict.data[i] = pixels[i];
			pict.linesize[i] = pitches[i];
		}

		currentFrame->CopyScaled(&pict, w, h, AV_PIX_FMT_YUYV422);
	}

	void updateBitmapBgr32(uint8_t* pixels, int w, int h)
	{
		if(currentFrame == 0){
			FlogE("Video::updateBitmapBgr32() called but currentFrame is unset");
			throw VideoException(VideoException::EScaling);
		}

		AVPixelFormat fmt = PIX_FMT_RGB32;

		AVPicture pict;
		int avret = avpicture_fill(&pict, pixels, fmt, w, h);
		
		if(avret < 0){
			FlogE("avpicture_fill returned " << avret);
			throw VideoException(VideoException::EScaling);
		}
		
		currentFrame->CopyScaled(&pict, w, h, fmt);
	}
	
	bool seekInternal(double t, int depth)
	{
		ResetRetries();
		emptyFrameQueue();
		audioHandler->clearQueue();

		int64_t firstTs = getFirstSeekTs();

		double backSeek = (double)depth * 2.0f + 1.0f;

		int64_t minTs = tsFromTime(t - backSeek - 2.5) + firstTs;
		int64_t ts = tsFromTime(t - backSeek) + firstTs;
		int64_t maxTs = tsFromTime(t - backSeek) + firstTs;

		// There is no discernible way to determine if negative timestamps are allowed
		// (or even required) to seek to low timestamps.
		// On some files you must seek to negative timestamps to be able to seek to 0
		// but on other files you get weird results from seeking to below 0.

		// So, every other try, we will allow seeking to negative timestamps.

		if((depth % 2) == 1){
			minTs = std::max((int64_t)0, minTs);
			ts = std::max((int64_t)0, minTs);
			maxTs = std::max((int64_t)0, minTs);
		}

		FlogD("Trying to seek to minTs: " << minTs << " ts: " << ts << " maxTs: " << maxTs << " with firsTs: " << firstTs);

		int flags = 0;
		
		if(ts < pFormatCtx->streams[videoStream]->cur_dts)
			flags |= AVSEEK_FLAG_BACKWARD;

		int seekRet = avformat_seek_file(pFormatCtx, videoStream, minTs, ts, maxTs, flags);

		if(seekRet > 0){
			FlogD("avformat_seek_file failed, returned " << seekRet);
			return false;
		}

		avcodec_flush_buffers(pCodecCtx);

		double newTime = t + timeFromTs(firstPts);
		double actualTime = skipToTs(newTime);

		// consider the seek failed and try again if the actual time diffs more than .5 seconds
		// from the desired new time. 
		
		FlogD("wanted to seek to " << newTime << " and ended up at " << actualTime);

		bool ret = true;

		if(fabsf(newTime - actualTime) > .5){
			if(depth < 5){
				FlogD("not good enough, trying again");
				return seekInternal(t, depth + 1);
			}
			else{
				ret = false;
				FlogW("seek failed, wanted to seek to " << newTime << " and ended up at " << actualTime);
			}
		}

		timeHandler->SetTime(actualTime);

		stepIntoQueue = true;

		audioHandler->onSeek();

		return ret;
	}

	bool seek(double ts){
		// if the video is currently playing,
		// pause it temporarily during seeking so that
		// not frames aren't dropped because of lost time during seeking
		
		bool tmpPause = !timeHandler->GetPaused();

		if(tmpPause){
			audioDevice->SetPaused(true);
			timeHandler->Pause();
		}

		bool ret = seekInternal(ts, 0);

		if(tmpPause){
			audioDevice->SetPaused(false);
			timeHandler->Play();
		}

		return ret;
	}

	/* Step to next frame */
	bool step(){
		float t = timeHandler->GetTime(), fps = getFrameRate();
		timeHandler->SetTime((t * fps + 1.0) / fps);
		return true;
	}

	bool stepBack(){
		float t = timeHandler->GetTime(), fps = getFrameRate();
		return seek(floorf(t * fps - 2.0));
	}
	
	double skipToTs(double ts)
	{
		double ret = -1000000000.0;

		for(int i = 0; i < 100; i++){
			// "tick" the video, filling up the frame queue
			tick(true);
				
			// throw away any frames below the requested time
			double curr = .0;
			while(frameQueue.GetContainer().size() > 0 && (curr = timeFromTs(frameQueue.GetContainer().at(0)->GetPts())) < ts){
				ret = curr;
				frameQueue.GetContainer().erase(frameQueue.GetContainer().begin());
			}

			// done if the frameQueue has a timestamp equal to or larger than the requested time
			if(frameQueue.size() > 0 && timeFromTs(frameQueue.top()->GetPts()) >= ts)
				break;
		}

		if(frameQueue.size() > 0){
			// return the actual timestamp achieved
			ret = timeFromTs(frameQueue.GetContainer().at(0)->GetPts());
			audioHandler->discardQueueUntilTs(ret);
		}
		
		return ret;
	}

	void tick(bool includeOldAudio = false){
		bool success = false;

		StreamFrameMap streamFrames;

		streamFrames[videoStream] = Frame::CreateEmpty();
		streamFrames[audioStream] = Frame::CreateEmpty();

		while(!IsEof() && !success)
		{
			try
			{
				int audioQueueTargetSize = audioDevice->GetBlockSize() * 4;

				while(
					frameQueue.size() < (unsigned int)targetFrameQueueSize || 
					(hasAudioStream() && audioHandler->getAudioQueueSize() < audioQueueTargetSize))
				{
					if(frameQueue.size() >= (unsigned int)maxFrameQueueSize)
						break;
					
					bool frameDecoded = decodeFrame(streamFrames);

					if(!frameDecoded)
						throw VideoException(VideoException::EDecodingVideo);

					if(streamFrames[videoStream]->finished != 0){
						frameQueue.push(streamFrames[videoStream]->Clone());
						streamFrames[videoStream] = Frame::CreateEmpty();
					}
					
					if(streamFrames[audioStream]->finished != 0){
						// only enqueue audio that's newer than the current video time, 
						// eg. on seeking we might encounter audio that's older than the frames in the frame queue.
						if(streamFrames[audioStream]->GetSamples().size() > 0 && 
							(includeOldAudio || streamFrames[audioStream]->GetSamples()[0].ts >= timeHandler->GetTime()))
						{
							audioHandler->EnqueueAudio(streamFrames[audioStream]->GetSamples());
						}else{
							FlogD("skipping old audio samples: " << streamFrames[audioStream]->GetSamples().size());
						}
						streamFrames[audioStream] = Frame::CreateEmpty();
					}
				}

				// sync framequeue target size with number of frames needed for audio queue 
				if(targetFrameQueueSize < (int)frameQueue.size()){
					targetFrameQueueSize = std::max((int)frameQueue.size(), minFrameQueueSize);
				}
					
				success = true;
			}

			catch(VideoException e)
			{
				Retry(Str("Exception in tick: " << e.what()));
			}
		}
	}
	
	void play(){
		audioDevice->SetPaused(false);
		timeHandler->Play();
	}

	int getWidth(){
		return w;
	}

	int getHeight(){
		return h;
	}

	double getDuration(){
		if(isValidTs(pFormatCtx->duration))
			return (double)pFormatCtx->duration / (double)AV_TIME_BASE;

		return 0;
	}

	float getFrameRate(){
		AVRational avFps = av_guess_frame_rate(pFormatCtx, pFormatCtx->streams[videoStream], NULL);
		return avFps.num != 0 && avFps.den != 0 ? (float)av_q2d(avFps) : 30.0f;
	}

	float getPAR(){
		if(pCodecCtx->sample_aspect_ratio.den != 0 && pCodecCtx->sample_aspect_ratio.num != 0)
			return (float)pCodecCtx->sample_aspect_ratio.num / pCodecCtx->sample_aspect_ratio.den; 
		return 1.0f;
	}

	double getPosition(){
		return std::max(lastFrameQueuePts - timeFromTs(firstPts), .0);
	}

	float getAspect(){
		return ((float)w * getPAR()) / h;
	}

	double timeFromTs(int64_t pts){
		return (double)pts * av_q2d(pFormatCtx->streams[videoStream]->time_base);
	}

	int64_t tsFromTime(double sec)
	{
		return sec / av_q2d(pFormatCtx->streams[videoStream]->time_base);
	}

	void setPlaybackSpeed(double speed){
		timeHandler->SetTimeWarp(speed);
	}

	PacketPtr demuxPacket()
	{
		PacketPtr packet = Packet::Create();

		do{
			// Read frames until we get a frame from the video or audio stream
			int ret = 0;
			if((ret = av_read_frame(pFormatCtx, &packet->avPacket)) < 0){
				Retry("av_read_frame failed in demuxPacket");
			}
		} while(packet->avPacket.stream_index != videoStream && packet->avPacket.stream_index != audioStream);

		return packet;
	}

	void decodePacket(PacketPtr packet, StreamFrameMap& streamFrames)
	{
		int bytesRemaining = packet->avPacket.size;

		// Decode until all bytes in the read frame is decoded
		while(bytesRemaining > 0)
		{
			int bytesDecoded = 0;
			int idx = packet->avPacket.stream_index;
			auto it = streamFrames.find(idx);

			if(it != streamFrames.end()){
				FramePtr frame = it->second;

				switch(pFormatCtx->streams[idx]->codec->codec_type){
					case AVMEDIA_TYPE_VIDEO:
						if( (bytesDecoded = avcodec_decode_video2(pCodecCtx, frame->GetAvFrame(), &frame->finished, &packet->avPacket)) <= 0 ){
							Retry(Str("avcodec_decode_video2() failed in decodePacket, returned: " << bytesDecoded));
						}
						frame->hasVideo = true;
						break;

					case AVMEDIA_TYPE_AUDIO:
						if((bytesDecoded = audioHandler->decode(packet->avPacket, pFormatCtx->streams[audioStream], frame, frame->finished)) <= 0){
							Retry(Str("audio decoder failed in decodePacket, returned: " << bytesDecoded));
						}
						frame->hasAudio = true;
						break;

					default:
						break;
				}
			}else{
				// stream not handled
				return;
			}

			bytesRemaining -= bytesDecoded;

			if(bytesRemaining > 0)
				Retry(Str("decodePacket not finished, bytesRemaining: " << bytesRemaining << ", bytesDecoded: " << bytesDecoded));
		}
	}

	bool decodeFrame(StreamFrameMap& streamFrames)
	{
		bool done = false;

		while(!done)
		{
			// demux
			PacketPtr packet = demuxPacket();
			if(packet == 0){
				FlogE("demuxing failed");
				return 0;
			}

			// decode
			decodePacket(packet, streamFrames);

			// check if any frames finished
			for(auto pair : streamFrames){
				FramePtr frame = pair.second;
				if(frame->finished != 0){
					// set timestamp and break out of loop
					int64_t pts = av_frame_get_best_effort_timestamp(frame->GetAvFrame());

					if(pair.first == videoStream){
						if(firstDts == AV_NOPTS_VALUE){
							firstDts = frame->GetAvFrame()->pkt_dts;
							FlogD("setting firstDts to: " << firstDts);
						}

						if(firstPts == AV_NOPTS_VALUE){
							firstPts = pts;
							FlogD("setting firstPts to: " << firstPts);
						}
					}
					
					frame->SetPts(pts);
					done = true;
				}
			}
		}
		
		// successfully decoded frame, reset retry counter
		ResetRetries();

		return true;
	}

	FramePtr decodeUntilVideoFrame(){
		StreamFrameMap streamFrames;
		streamFrames[videoStream] = Frame::CreateEmpty();

		for(int i = 0; i < 100; i++){
			try {
				bool ret = decodeFrame(streamFrames);

				if(!ret){
					FlogW("failed to decode frame");
					return false;
				}

				// throw away any resulting frames
				if(streamFrames[videoStream]->finished != 0)
					return streamFrames[videoStream];
			}

			catch(VideoException e)
			{
				FlogW("While decoding video frame");
				FlogW(e.what());
			}

			Retry("not a video frame in decodeUntilVideoFrame()");
		}

		FlogD("couldn't find a video frame in 100 steps");
		return 0;
	}

	int64_t getFirstSeekTs()
	{
		return pFormatCtx->iformat->flags & AVFMT_SEEK_TO_PTS ? firstPts : firstDts;
	}

	void emptyFrameQueue(){
		while(!frameQueue.empty()){
			frameQueue.pop();
		}
	}

	bool hasAudioStream()
	{
		return audioStream != AVERROR_STREAM_NOT_FOUND && audioStream != AVERROR_DECODER_NOT_FOUND;
	}

	void openFile(StreamPtr stream, IAudioDevicePtr audioDevice)
	{
		FlogI("Trying to load file: " << stream->GetPath());

		int ret;
		this->stream = stream;
		this->audioDevice = audioDevice;
		timeHandler = TimeHandler::Create(audioDevice);
		
		audioDevice->SetPaused(true);

		pFormatCtx = avformat_alloc_context();
		pFormatCtx->pb = stream->GetAVIOContext();

		if((ret = avformat_open_input(&pFormatCtx, stream->GetPath().c_str(), NULL, NULL)) != 0){
			char ebuf[512];
			av_strerror(ret, ebuf, sizeof(ebuf));
			FlogE("couldn't open file");
			FlogE(ebuf);
			throw VideoException(VideoException::EFile);
		}

		/* Get stream information */
		if(avformat_find_stream_info(pFormatCtx, NULL) < 0){
			FlogE("couldn't get stream info");
			throw VideoException(VideoException::EStreamInfo);
		}

		/* Print video format information */
		av_dump_format(pFormatCtx, 0, stream->GetPath().c_str(), 0);

		// If the loader logged something about wmv being DRM protected, give up
		if(drm){
			throw VideoException(VideoException::EStream);
		}

		// find the best audio and video streams
		audioStream = videoStream = AVERROR_STREAM_NOT_FOUND;
		pCodec = 0;

		videoStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);

		if(videoStream == AVERROR_STREAM_NOT_FOUND){
			FlogE("couldn't find stream");
			throw VideoException(VideoException::EStream);
		}	
		
		if(videoStream == AVERROR_DECODER_NOT_FOUND || !pCodec){
			FlogE("unsupported codec");
			throw VideoException(VideoException::EVideoCodec);
		}

		audioStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

		if(hasAudioStream()){
			audioHandler = AudioHandler::Create(pFormatCtx->streams[audioStream]->codec, audioDevice, timeHandler);
		}else{
			audioHandler = AudioHandlerNoSound::Create(audioDevice, timeHandler);
			FlogD("no audio stream or unsupported audio codec");
		}
		
		/* Get a pointer to the codec context for the video stream */
		pCodecCtx = pFormatCtx->streams[videoStream]->codec;

		// Open codec
		if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0){
			FlogE("unsupported codec");
			throw VideoException(VideoException::EVideoCodec);
		}

		w = pCodecCtx->width;
		h = pCodecCtx->height;

		// limit framequeue memory size
		int frameMemSize = avpicture_get_size((AVPixelFormat)pCodecCtx->pix_fmt, w, h);
		int maxVideoQueueMem = 512 * 1024 * 1024; // 512 MB
		maxFrameQueueSize = maxVideoQueueMem / frameMemSize;

		// cap to 256
		maxFrameQueueSize = std::min(maxFrameQueueSize, 256);

		// tick the video so that firstPts and firstDts are set
		tick(true);
	}

	void closeFile(){
		if(pCodecCtx)
			avcodec_close(pCodecCtx);

		// force audio handler to close its codec
		audioHandler = 0;

		if(pFormatCtx){
			pFormatCtx->pb = 0;
			avformat_close_input(&pFormatCtx);
		}

		stream->Close();

		messageCallback(MUnloadedFile, stream->GetPath());
	}

	bool IsEof(){
		return retryStack.size() > maxRetries;
	}

	void Retry(std::string desc)
	{
		retryStack.push_back(desc);
		if(retryStack.size() > maxRetries){
			FlogW("Maximum number of retries reached, they were spent on:");

			int repeat = 0;
			std::string lastStr = "";

			for(auto str : retryStack){
				if(lastStr == str){
					repeat++;
				}else{
					if(repeat > 0)
						FlogW("(repeats " << repeat << " times)");

					FlogW(" * " << str);
					repeat = 0;
				}

				lastStr = str;
			}

			if(repeat > 0)
					FlogW("(repeats " << repeat << " times)");

			throw VideoException(VideoException::ERetries);
		}
	}

	void ResetRetries()
	{
		retryStack.clear();
		reportedEof = false;
	}

	void pause(){
		audioDevice->SetPaused(true);
		timeHandler->Pause();
	}

	bool getPaused(){
		return timeHandler->GetPaused();
	}

	void SetVolume(float volume)
	{
		audioHandler->SetVolume(volume);
	}

	void SetMute(bool mute)
	{
		audioHandler->SetMute(mute);
	}

	void SetQvMute(bool qvMute)
	{
		audioHandler->SetQvMute(qvMute);
	}

	static void logCb(void *ptr, int level, const char *fmt, va_list vargs)
	{
		if(level == AV_LOG_WARNING){

			/* HACK, can we extract this information from the headers structures somehow? */

			if(!strcmp(fmt, "DRM protected stream detected, decoding will likely fail!\n")){
				FlogI("DRM protected stream");
				drm = true;
			}

			else if(!strcmp(fmt, "Ext DRM protected stream detected, decoding will likely fail!\n")){
				FlogI("Ext DRM protected stream");
				drm = true;
			}

			else if(!strcmp(fmt, "Digital signature detected, decoding will likely fail!\n")){
				FlogI("Digitally signed stream");
				drm = true;
			}
		}

		if (level <= av_log_get_level()){
			char tmp[1024];
			vsprintf(tmp, fmt, vargs);

			FlogD("ffmpeg says: " << tmp);
		}
	}
};

bool CVideo::drm = false;

VideoPtr Video::Create(StreamPtr stream, MessageCallback messageCallback, IAudioDevicePtr audioDevice)
{
	static bool initialized = false;
	if(!initialized)
		av_register_all();

	CVideo* video = new CVideo(messageCallback);

	av_log_set_callback(CVideo::logCb);
	av_log_set_level(AV_LOG_WARNING);

	try {
		video->openFile(stream, audioDevice);
	}

	catch(VideoException e){
		delete video;
		throw e;
	}

	return VideoPtr(video);
}
