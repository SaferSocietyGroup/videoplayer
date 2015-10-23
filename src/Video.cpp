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

#include "Video.h"
#include "Flog.h"
#include "AudioHandler.h"
#include "AudioHandlerNoSound.h"
#include "TimeHandler.h"
#include "PriorityQueue.h"
#include "Frame.h"
#include "Packet.h"

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

	AVFormatContext* pFormatCtx = 0;
	AVCodecContext* pCodecCtx = 0;
	AVCodec *pCodec = 0;
	
	int duration = 0;
	int reachedEof = false;
	int maxFrameQueueSize = 0;
	int minFrameQueueSize = 16;

	PriorityQueue<FramePtr, CompareFrames> frameQueue;
	
	double lastDecodedPts = .0;
	double lastFrameQueuePts = .0;
	int64_t firstPts = 0;
	bool firstPtsSet = false;

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

		FramePtr newFrame = 0;

		// Throw away all old frames (timestamp older than now) except for the last
		// and set the pFrame pointer to that.

		while(!frameQueue.empty() && timeFromPts(frameQueue.top()->GetPts()) < time)
		{
			newFrame = frameQueue.top();
			frameQueue.pop();
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
		reachedEof = 0;
		reportedEof = false;
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

		timeHandler->SetTime(lastDecodedPts);
		stepIntoQueue = true;

		reachedEof = 0;
		reportedEof = false;

		audioHandler->onSeek();

		FlogExpD(reachedEof);

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
		while(!IsEof() && !success)
		{
			try
			{
				while(
					frameQueue.size() < (unsigned int)minFrameQueueSize || 
					(hasAudioStream() && audioHandler->getAudioQueueSize() < audioDevice->GetBlockSize()))
				{
					if(frameQueue.size() >= (unsigned int)maxFrameQueueSize)
						break;
					
					FramePtr frame = decodeFrame();
					if(frame->type == Frame::TVideo){
						frameQueue.push(frame->Clone());
					}else{
						audioHandler->EnqueueAudio(frame->GetSamples());
					}
				}
					
				success = true;
				reachedEof = 0;
				reportedEof = false;
			}

			catch(VideoException e)
			{
				reachedEof++;
				if(IsEof())
				{
					FlogExpD(reachedEof);
				}
			}
		}
	}
	
	void play(){
		timeHandler->Play();
	}

	int getWidth(){
		return w;
	}

	int getHeight(){
		return h;
	}

	int getDurationInFrames(){
		return std::max(getReportedDurationInFrames(), 0);
	}

	// duration as reported by ffmpeg/the video's header
	int getReportedDurationInFrames(){
		return (int)(((double)pFormatCtx->duration / 
			(double)AV_TIME_BASE) * (double)getReportedFrameRate());
	}

	double getReportedDurationInSecs(){
		if(isValidTs(pFormatCtx->duration))
			return (double)pFormatCtx->duration / (double)AV_TIME_BASE;

		return 0;
	}

	double getDurationInSecs(){
		if(duration > -1)
			return (double)duration / (double)getFrameRate();

		return getReportedDurationInSecs();
	}

	float getFrameRate(){
		// if a framerate has been calculated, use that, otherwise the one reported in the header
		// return avgFrameRate > 0 ? avgFrameRate : getReportedFrameRate();

		// NOTE this used to be calculated in genCollage, which has been removed because it wasn't used.
		// Since we've been getting by without this value for quite a while it's probably safe to assume
		// that it's not needed anymore.

		return getReportedFrameRate();
	}

	float getReportedFrameRate(){
		AVRational avgfps = pFormatCtx->streams[videoStream]->avg_frame_rate;
		return avgfps.num != 0 && avgfps.den != 0 ? (float)av_q2d(pFormatCtx->streams[videoStream]->avg_frame_rate) : 30.0f;
	}

	float getPAR(){
		if(pCodecCtx->sample_aspect_ratio.den != 0 && pCodecCtx->sample_aspect_ratio.num != 0)
			return (float)pCodecCtx->sample_aspect_ratio.num / pCodecCtx->sample_aspect_ratio.den; 
		return 1.0f;
	}

	double getPosition(){
		return std::max(lastFrameQueuePts - timeFromPts(firstPts), .0);
	}

	float getAspect(){
		return ((float)w * getPAR()) / h;
	}

	std::string getVideoCodecName(){
		return pCodec ? pCodec->name : "none";
	}

	std::string getFormat(){
		return pFormatCtx->iformat->name;
	}

	double timeFromPts(uint64_t pts){
		return (double)pts * av_q2d(pFormatCtx->streams[videoStream]->time_base);
	}

	int64_t tsFromTime(double sec)
	{
		return sec / av_q2d(pFormatCtx->streams[videoStream]->time_base);
	}
	
	int getVideoBitRate(){
		return pCodecCtx->bit_rate;
	}

	static bool drm;

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

	bool decodePacket(PacketPtr packet, FramePtr decFrame, int& frameFinished)
	{
		int bytesRemaining = packet->avPacket.size;
		int bytesDecoded = 0;
		int decodeTries = 0;

		// Decode until all bytes in the read frame is decoded
		while(bytesRemaining > 0)
		{
			if(packet->avPacket.stream_index == videoStream)
			{
				// Decode video
				if( (bytesDecoded = avcodec_decode_video2(pCodecCtx, decFrame->GetAvFrame(), &frameFinished, &packet->avPacket)) < 0 )
					FlogW("avcodec_decode_video2() failed, trying again");

				decFrame->type = Frame::TVideo;
			}

			else
			{
				if((bytesDecoded = audioHandler->decode(packet->avPacket, pFormatCtx->streams[audioStream], 
						timeHandler->GetTimeWarp(), decFrame, frameFinished)) <= 0){
					FlogW("audio decoder failed, trying again");
				}

				decFrame->type = Frame::TAudio;
			}

			if(decodeTries++ > 1000)
				return false;

			bytesRemaining -= bytesDecoded;
		}

		return true;
	}

	FramePtr decodeFrame()
	{
		int frameFinished = 0;
		FramePtr decFrame = Frame::Create(avcodec_alloc_frame(), (uint8_t*)0, 0, true);

		while(frameFinished == 0)
		{
			PacketPtr packet = demuxPacket();
			if(packet == 0){
				FlogE("demuxing failed");
				return 0;
			}

			if(!decodePacket(packet, decFrame, frameFinished)){
				FlogE("decoding failed");
				return 0;
			}
		}
		
		int64_t pts = av_frame_get_best_effort_timestamp(decFrame->GetAvFrame());
		decFrame->SetPts(pts);
		
		if(decFrame->type == Frame::TVideo){
			this->lastDecodedPts = timeFromPts(pts);

			if(!firstPtsSet || pts < firstPts){
				firstPts = pts;
				firstPtsSet = true;
			}
		}

		return decFrame;
	}

	bool decodeVideoFrame(){
		for(int i = 0; i < 100; i++){
			try {
				FramePtr frame = decodeFrame();
				if(frame->type == Frame::TVideo){
					return true;
				}
			}
			catch(VideoException e)
			{
				FlogW("While decoding video frame");
				FlogW(e.what());
			}
		}

		FlogD("couldn't find a video frame in 100 steps");
		return false;
	}

	// Raw byte seeking
	bool seekRaw(int frame){
		double to = (double)frame / (double)getDurationInFrames();
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
		FlogExpD(firstPts);
		FlogExpD(tsFromTime(firstPts));
		FlogExpD(t);
		FlogExpD(tsFromTime(t));

		// work around for h.264/mp4 files not seeking to 0
		if(t <= 0.0)
			t = -1;

		//int flags = 0;

		//if(t < timeHandler->GetTime())
		//	flags |= AVSEEK_FLAG_BACKWARD;

		//int seekRet = av_seek_frame(pFormatCtx, videoStream, tsFromTime(t) + firstPts, flags);

		int64_t minTs = tsFromTime(t - 5.0) + firstPts;
		int64_t maxTs = tsFromTime(t) + firstPts;
		int64_t ts = tsFromTime(t) + firstPts;

		int seekRet = avformat_seek_file(pFormatCtx, videoStream, minTs, ts, maxTs, AVSEEK_FLAG_ANY);

		if(seekRet > 0){
			FlogD("avformat_seek_file failed, returned " << seekRet);
			return false;
		}

		avcodec_flush_buffers(pCodecCtx);

		do{
			if(!decodeVideoFrame())
				return false;

		}while(lastDecodedPts < t + timeFromPts(firstPts));

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
		
		timeHandler->Pause();

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

		// cap to 128
		maxFrameQueueSize = std::min(maxFrameQueueSize, 128);
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
		return reachedEof > 100;
	}

	int getSampleRate(){
		return audioHandler->getSampleRate();
	}

	int getNumChannels(){
		return audioHandler->getChannels();
	}
	void pause(){
		timeHandler->Pause();
	}

	bool getPaused(){
		return timeHandler->GetPaused();
	}

	double getTime(){
		return timeHandler->GetTime();
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
};

bool CVideo::drm = false;

static void logCb(void *ptr, int level, const char *fmt, va_list vargs)
{
	if(level == AV_LOG_WARNING){

		/* HACK, can we extract this information from the headers structures somehow? */

		if(!strcmp(fmt, "DRM protected stream detected, decoding will likely fail!\n")){
			FlogI("DRM protected stream");
			CVideo::drm = true;
		}

		else if(!strcmp(fmt, "Ext DRM protected stream detected, decoding will likely fail!\n")){
			FlogI("Ext DRM protected stream");
			CVideo::drm = true;
		}

		else if(!strcmp(fmt, "Digital signature detected, decoding will likely fail!\n")){
			FlogI("Digitally signed stream");
			CVideo::drm = true;
		}
	}

	if (level <= av_log_get_level()){
		char tmp[1024];
		vsprintf(tmp, fmt, vargs);

		FlogD("ffmpeg says: " << tmp);
	}
}

VideoPtr Video::Create(StreamPtr stream, MessageCallback messageCallback, IAudioDevicePtr audioDevice)
{
	static bool initialized = false;
	if(!initialized)
		av_register_all();

	CVideo* video = new CVideo(messageCallback);

	av_log_set_callback(logCb);
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
