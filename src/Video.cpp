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
	int maxRetries = 100;

	AVFormatContext* pFormatCtx = 0;
	AVCodecContext* pCodecCtx = 0;
	AVCodec *pCodec = 0;
	
	static bool drm;
	
	int retries = 0;
	int maxFrameQueueSize = 0;
	int minFrameQueueSize = 16;
	int targetFrameQueueSize = 16;

	PriorityQueue<FramePtr, CompareFrames> frameQueue;
	
	double lastFrameQueuePts = .0;

	FramePtr currentFrame = 0;
	float t = 0.0f;
	bool reportedEof = false;
	StreamPtr stream;
	
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
		if(frameQueue.empty() && IsEof() && !reportedEof)
		{
			reportedEof = true;
			messageCallback(MEof, "eof");
		}

		if(stepIntoQueue && !frameQueue.empty())
		{
			stepIntoQueue = false;
			timeHandler->SetTime(timeFromPts(frameQueue.top()->GetPts()) + .001);
		}

		double time = timeHandler->GetTime();

		//FlogD("time: " << time << ", queue: " << timeFromPts(frameQueue.top()->GetPts()));

		FramePtr newFrame = 0;

		// Throw away all old frames (timestamp older than now) except for the last
		// and set the pFrame pointer to that.
		int poppedFrames = 0;

		while(!frameQueue.empty() && timeFromPts(frameQueue.top()->GetPts()) < time)
		{
			newFrame = frameQueue.top();
			frameQueue.pop();
			poppedFrames++;
		}
			
		if(poppedFrames > 1){
			FlogD("skipped " << poppedFrames - 1 << " frames");
		}

		return newFrame;
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
		double pts = timeFromPts(frameQueue.top()->GetPts());

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

			lastFrameQueuePts = timeFromPts(newFrame->GetPts());
					
			return true;
		}

		return false;
	}
	
	void updateOverlay(uint8_t** pixels, const uint16_t* pitches, int w, int h)
	{
		PixelFormat fffmt = AV_PIX_FMT_YUYV422;
		AVPicture pict;

		int avret = avpicture_fill(&pict, NULL, fffmt, w, h);

		if(avret < 0){
			FlogE("avpicture_fill returned " << avret);
			throw std::runtime_error("avpicture_fill error");
		}

		for(int i = 0; i < 3; i++){
			pict.data[i] = pixels[i];
			pict.linesize[i] = pitches[i];
		}

		struct SwsContext* swsCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, 
				pCodecCtx->pix_fmt, w, h, fffmt, SWS_BILINEAR, NULL, NULL, NULL);

		if(swsCtx){
			sws_scale(swsCtx, (uint8_t**)currentFrame->GetAvFrame()->data, currentFrame->GetAvFrame()->linesize, 0, 
				pCodecCtx->height, pict.data, pict.linesize); 
			sws_freeContext(swsCtx);
		}else{
			FlogE("Failed to get a scaling context");
		}
	}

	bool seek(double ts){
		emptyFrameQueue();
		audioHandler->clearQueue();

		if(!seekTs(ts)){
			// Try to seek with raw byte seeking
			if(!seekRaw(ts)){
				FlogD("seek failed");
				return false;
			}else{
				FlogD("used seekRaw");
			}
		}else{
			FlogD("used seekTs");
		}

		double lastDecodedPts = .0;
		FlogExpD(ts - timeFromPts(pFormatCtx->streams[videoStream]->start_time));

		//FlogD(to);
		
		do{
			FramePtr frame;

			if((frame = decodeVideoFrame()) == 0){
				FlogE("failed to decode video frame");
				return false;
			}

			lastDecodedPts = timeFromPts(frame->GetPts());
			FlogExpD(lastDecodedPts);

		}while(lastDecodedPts < t + timeFromPts(pFormatCtx->streams[videoStream]->start_time));

		FlogExpD(lastDecodedPts);

		timeHandler->SetTime(lastDecodedPts);
		stepIntoQueue = true;

		audioHandler->onSeek();

		return true;
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

	void tick(){
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
						audioHandler->EnqueueAudio(streamFrames[audioStream]->GetSamples());
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
				Retry();
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
		AVRational avgfps = pFormatCtx->streams[videoStream]->avg_frame_rate;
		return avgfps.num != 0 && avgfps.den != 0 ? (float)av_q2d(pFormatCtx->streams[videoStream]->avg_frame_rate) : 30.0f;
	}

	float getPAR(){
		if(pCodecCtx->sample_aspect_ratio.den != 0 && pCodecCtx->sample_aspect_ratio.num != 0)
			return (float)pCodecCtx->sample_aspect_ratio.num / pCodecCtx->sample_aspect_ratio.den; 
		return 1.0f;
	}

	double getPosition(){
		return std::max(lastFrameQueuePts - timeFromPts(pFormatCtx->streams[videoStream]->start_time), .0);
	}

	float getAspect(){
		return ((float)w * getPAR()) / h;
	}

	double timeFromPts(uint64_t pts){
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
		int tries = 0;
		PacketPtr packet = Packet::Create();

		do{
			// Read frames until we get a frame from the video or audio stream
			int ret = 0;
			if((ret = av_read_frame(pFormatCtx, &packet->avPacket)) < 0){
				if(tries++ > 100)
					return 0;
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
						if( (bytesDecoded = avcodec_decode_video2(pCodecCtx, frame->GetAvFrame(), &frame->finished, &packet->avPacket)) < 0 ){
							FlogW("avcodec_decode_video2() failed, trying again");
							Retry();
						}
						frame->hasVideo = true;
						break;

					case AVMEDIA_TYPE_AUDIO:
						if((bytesDecoded = audioHandler->decode(packet->avPacket, pFormatCtx->streams[audioStream], 
										timeHandler->GetTimeWarp(), frame, frame->finished)) <= 0){
							FlogW("audio decoder failed, trying again");
							Retry();
						}
						frame->hasAudio = true;
						break;

					default:
						break;
				}
			}

			bytesRemaining -= bytesDecoded;

			Retry();
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
					frame->SetPts(pts);
					done = true;
				}
			}

			Retry();
		}
		
		// successfully decoded frame, reset retry counter
		ResetRetries();

		return true;
	}

	FramePtr decodeVideoFrame(){
		StreamFrameMap streamFrames;
		streamFrames[videoStream] = Frame::CreateEmpty();

		for(int i = 0; i < 100; i++){
			try {
				bool ret = decodeFrame(streamFrames);

				if(!ret){
					FlogW("failed to decode frame");
					return false;
				}
			}

			catch(VideoException e)
			{
				FlogW("While decoding video frame");
				FlogW(e.what());
			}

			Retry();
		}

		FlogD("couldn't find a video frame in 100 steps");
		return 0;
	}

	// Raw byte seeking
	bool seekRaw(double ts){
		double to = ts / getDuration() - 1.0;
		int64_t fileSize = avio_size(pFormatCtx->pb);
		if(av_seek_frame(pFormatCtx, -1, (int64_t)((double)fileSize * to), AVSEEK_FLAG_BYTE) < 0){
			FlogE("Raw seeking error");
			return false;
		}

		avcodec_flush_buffers(pCodecCtx);
		decodeVideoFrame();
		return true;
	}

	// Seek by timestamp
	bool seekTs(double t){
		int64_t firstPts = pFormatCtx->streams[videoStream]->start_time;

		FlogExpD(firstPts);
		FlogExpD(tsFromTime(firstPts));
		FlogExpD(t);
		FlogExpD(tsFromTime(t));

		int64_t minTs = tsFromTime(std::max(t - 5.0, .0)) + firstPts;
		int64_t maxTs = tsFromTime(t) + firstPts;
		int64_t ts = tsFromTime(t) + firstPts;
		
		int flags = AVSEEK_FLAG_ANY;

		if(ts < pFormatCtx->streams[videoStream]->cur_dts)
			flags |= AVSEEK_FLAG_BACKWARD;

		FlogExpD(flags | AVSEEK_FLAG_BACKWARD);

		int seekRet = avformat_seek_file(pFormatCtx, videoStream, minTs, ts, maxTs, flags);

		if(seekRet > 0){
			FlogD("avformat_seek_file failed, returned " << seekRet);
			return false;
		}

		avcodec_flush_buffers(pCodecCtx);

		return true;
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

		FlogExpD(pFormatCtx->streams[videoStream]->r_frame_rate.num);
		FlogExpD(pFormatCtx->streams[videoStream]->r_frame_rate.den);
		FlogExpD(pFormatCtx->duration / AV_TIME_BASE);

		// limit framequeue memory size
		int frameMemSize = avpicture_get_size((AVPixelFormat)pCodecCtx->pix_fmt, w, h);
		int maxVideoQueueMem = 512 * 1024 * 1024; // 512 MB
		maxFrameQueueSize = maxVideoQueueMem / frameMemSize;

		FlogExpD(maxFrameQueueSize);
		FlogExpD(frameMemSize);

		// cap to 256
		maxFrameQueueSize = std::min(maxFrameQueueSize, 256);
		FlogExpD(maxFrameQueueSize);
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
		return retries > maxRetries;
	}

	void Retry()
	{
		if(retries++ > maxRetries)
			throw VideoException(VideoException::ERetries);
	}

	void ResetRetries()
	{
		retries = 0;
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
