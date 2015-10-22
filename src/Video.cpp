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

typedef std::shared_ptr<class Frame> FramePtr;

struct Frame
{
	AVFrame* avFrame;
	uint8_t* buffer;
	double pts;

	Frame(AVFrame* avFrame, uint8_t* buffer, double pts) : avFrame(avFrame), buffer(buffer), pts(pts)
	{
	}
	
	~Frame()
	{
		if(avFrame != 0)
			av_frame_free(&avFrame);

		if(buffer != 0)
			av_free(buffer);
	}
};
 
class CompareFrames
{
	public:
	bool operator()(FramePtr a, FramePtr b) const
	{
		return a->pts > b->pts;
	}
};

std::string VideoException::what()
{
	if((int)errorCode < 0 || (int)errorCode > (int)ESeeking){
		return "unknown video exception";
	}

	std::vector<std::string> eStr = {
		"file error",
		"video codec error",
		"stream info error",
		"stream error",
		"demuxing error",
		"decoding video error",
		"decoding audio error",
		"seeking error",
	};

	return eStr[(int)errorCode];
}

VideoException::VideoException(ErrorCode errorCode){
	this->errorCode = errorCode;
}

struct PanicException : public std::runtime_error {
	PanicException(std::string str) : std::runtime_error(str) {}
};

class CVideo : public Video
{
	public:
	enum FrameType
	{
		TVideo,
		TAudio
	};

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
	
	AVPicture pict;
	AVPacket packet;

	int duration = 0;
	int reachedEof = false;
	int maxFrameQueueSize = 0;
	int minFrameQueueSize = 16;

	PriorityQueue<FramePtr, CompareFrames> frameQueue;
	
	double lastDecodedPts = .0;
	double lastFrameQueuePts = .0;
	int64_t lastDts = 0;
	int64_t firstPts = 0;

	AVFrame* decFrame = 0;

	FramePtr currentFrame = 0;
	float t = 0.0f;
	bool reportedEof = false;
	StreamPtr stream;
	
	CVideo(MessageCallback messageCallback){
		this->messageCallback = messageCallback;
		this->maxFrameQueueSize = maxFrameQueueSize;
		memset(&packet, 0, sizeof(AVPacket));
		decFrame = avcodec_alloc_frame();
	}

	~CVideo(){
		closeFile();
		emptyFrameQueue();
		av_free(decFrame);
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
			timeHandler->SetTime(frameQueue.top()->pts + .001);
		}

		double time = timeHandler->GetTime();

		FramePtr newFrame;

		// Throw away all old frames (timestamp older than now) except for the last
		// and set the pFrame pointer to that.

		while(!frameQueue.empty() && frameQueue.top()->pts < time)
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
		double pts = frameQueue.top()->pts;

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

		if(newFrame != 0 && newFrame->avFrame){
			// Don't free currentFrame if it is currentFrame itself that's being converted
			if(currentFrame == 0 || currentFrame->avFrame != newFrame->avFrame){
				currentFrame = newFrame; // Save the current frame for snapshots etc.
			}

			lastFrameQueuePts = newFrame->pts;
					
			return true;
		}

		return false;
	}
	
	void updateOverlay(uint8_t** pixels, const uint16_t* pitches, int w, int h)
	{
		PixelFormat fffmt = AV_PIX_FMT_YUYV422;
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
			sws_scale(swsCtx, (uint8_t**)currentFrame->avFrame->data, currentFrame->avFrame->linesize, 0, pCodecCtx->height, pict.data, pict.linesize); 
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
					
					decodeFrame(true);
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

	void freePacket(){
		av_free_packet(&packet);
		memset(&packet, 0, sizeof(AVPacket));
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
		FlogExpD(av_q2d(pFormatCtx->streams[videoStream]->time_base));
		return sec / av_q2d(pFormatCtx->streams[videoStream]->time_base);
	}
	
	int getVideoBitRate(){
		return pCodecCtx->bit_rate;
	}

	static bool drm;

	void setPlaybackSpeed(double speed){
		timeHandler->SetTimeWarp(speed);
	}

	void demux()
	{
		do{
			// Read frames until we get a frame from the video or audio stream
			freePacket();
			int ret = 0;
			if((ret = av_read_frame(pFormatCtx, &packet)) < 0){
				freePacket();
				throw VideoException(VideoException::EDemuxing);
			}
		} while(packet.stream_index != videoStream && packet.stream_index != audioStream);
	}

	FrameType decodeFrameWithRetries(bool addToQueue, bool initialDemux = true)
	{
		int retries = 30;

		while(true){
			try
			{
				FrameType ret = decodeFrame(addToQueue, initialDemux);
				return ret;
			}

			catch(VideoException e)
			{
				if(retries < 1){
					throw e;
				}
			}

			retries--;
		}

		// never gets here
	}

	FrameType decodeFrame(bool addToQueue, bool initialDemux = true)
	{
		int frameFinished = 0;
		FrameType ret = TAudio;

		bool doDemux = initialDemux;

		while(frameFinished == 0)
		{
			if(doDemux)
				demux();

			doDemux = true;

			int bytesRemaining = packet.size;
			int bytesDecoded = 0;
			int decodeTries = 0;

			// Decode until all bytes in the read frame is decoded
			while(bytesRemaining > 0)
			{
				if(packet.stream_index == videoStream)
				{
					// Decode video
					if( (bytesDecoded = avcodec_decode_video2(pCodecCtx, decFrame, &frameFinished, &packet)) < 0 )
					{
						throw VideoException(VideoException::EDecodingVideo);
					}

					lastDts = packet.dts;

					ret = TVideo;
				}

				else
				{
					if((bytesDecoded = audioHandler->decode(packet, pFormatCtx->streams[audioStream], timeHandler->GetTimeWarp(), addToQueue)) <= 0)
					{
						throw VideoException(VideoException::EDecodingAudio);
					}
				}

				if(decodeTries++ > 1024){
					throw VideoException(VideoException::EDecodingVideo);
				}
				
				bytesRemaining -= bytesDecoded;
			}
		}
		
		int64_t pts = av_frame_get_best_effort_timestamp(decFrame);
		this->lastDecodedPts = timeFromPts(av_frame_get_best_effort_timestamp(decFrame));

		if(firstPts == 0 || pts < firstPts)
			firstPts = pts;

		if(addToQueue && ret == TVideo){
			frameQueue.push(cloneFrame(decFrame, this->lastDecodedPts));
		}

		return ret;
	}

	bool decodeVideoFrame(){
		for(int i = 0; i < 100; i++){
			if(decodeFrameWithRetries(false) == TVideo){
				return true;
			}
		}

		FlogD("couldn't find a video frame in 100 steps");
		return false;
	}

	FramePtr cloneFrame(AVFrame* src, double pts){
		AVFrame* avFrame = av_frame_alloc();
		uint8_t *buffer = (uint8_t *)av_malloc(avpicture_get_size((AVPixelFormat)src->format, src->width, src->height));

		if(!buffer || !avFrame){
			if(avFrame)
				av_frame_free(&avFrame);

			if(buffer)
				av_free(buffer);

			throw std::runtime_error("allocation failed in cloneframe");
		}

		avpicture_fill((AVPicture *) avFrame, buffer, pCodecCtx->pix_fmt, src->width, src->height);
		av_picture_copy((AVPicture*)avFrame, (AVPicture*)decFrame, pCodecCtx->pix_fmt, src->width, src->height);

		return std::make_shared<Frame>(avFrame, buffer, pts);
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
		FlogExpD(t);
		FlogExpD(tsFromTime(t));

		// work around for h.264/mp4 files not seeking to 0
		if(t <= 0.0)
			t = -1;

		int seekRet = av_seek_frame(pFormatCtx, videoStream, tsFromTime(t) + firstPts, AVSEEK_FLAG_ANY);

		if(seekRet > 0){
			FlogD("av_seek_frame failed, returned " << seekRet);
			return false;
		}

		avcodec_flush_buffers(pCodecCtx);

		bool ret = decodeVideoFrame();

		return ret;
	}

	bool seekKf(int frame){
		return false;
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
		freePacket();

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

	// frame = frame number
	// returns false on error
	bool decodePacket(int& frame, bool& isKeyFrame, bool& needsMorePackets){
		int isFinished = 0;
		int ret = avcodec_decode_video2(pCodecCtx, decFrame, &isFinished, &packet);
		
		if(ret < 0){
			return false;
		}

		if(isFinished){
			frame = duration;
			isKeyFrame = (decFrame->key_frame != 0);
			needsMorePackets = false;
			duration++;
			return true;
		}

		needsMorePackets = true;
		return true;
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
