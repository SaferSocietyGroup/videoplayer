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

struct Frame
{
	Frame(AVFrame* avFrame = 0, double pts = 0) 
		: avFrame(avFrame), pts(pts) {}

	AVFrame* avFrame;
	double pts;
	
	bool operator()(const Frame& a, const Frame& b) const
	{
		return a.pts > b.pts;
	}
};

class CVideo : public Video
{
	public:
	enum FrameType
	{
		TVideo,
		TAudio
	};

	ErrorCallback errorCallback;
	bool stepIntoQueue;

	int w, h;
	int backSeek;
	
	IAudioDevicePtr audioDevice;
	AudioHandlerPtr audioHandler;
	TimeHandlerPtr timeHandler;

	AVFormatContext *pFormatCtx;
	int audioStream, videoStream;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;
	
	AVPicture pict;
	AVPacket packet;

	int duration;
	int reachedEof;
	int frameQueueSize;

	Error lastError;

	PriorityQueue<Frame> frameQueue;
	
	double lastPts;
	int64_t lastDts;
	int64_t firstPts;

	AVFrame* decFrame;

	Frame currentFrame;
	float t;
	bool drawTimeStamp;
	bool reportedEof;
	StreamPtr stream;
	
	CVideo(ErrorCallback errorCallback, int frameQueueSize){
		this->errorCallback = errorCallback;

		this->frameQueueSize = frameQueueSize;
		reachedEof = 0;
		pCodec = 0;
		pCodecCtx = 0;
		pFormatCtx = 0;
		lastError = ENoError;
		drawTimeStamp = true;
		memset(&packet, 0, sizeof(AVPacket));
		lastPts = 0;
		lastDts = 0;
		firstPts = 0;
		t = 0.0f;
		stepIntoQueue = true;
		reportedEof = false;

		decFrame = avcodec_alloc_frame();
	}

	~CVideo(){
		FlogD("destructor");

		closeFile();

		emptyFrameQueue();

		av_free(decFrame);
		av_frame_free(&currentFrame.avFrame);

		FlogD("end of destructor");
	}

	Frame fetchFrame(){
		if(frameQueue.empty() && IsEof() && !reportedEof){
			reportedEof = true;
			errorCallback(EEof, "eof");
		}

		if(stepIntoQueue && !frameQueue.empty()){
			stepIntoQueue = false;
			timeHandler->SetTime(frameQueue.top().pts + .001);
		}

		double time = timeHandler->GetTime();

		Frame newFrame;

		// Throw away all old frames (timestamp older than now) except for the last
		// and set the pFrame pointer to that.

		while(!frameQueue.empty() && frameQueue.top().pts < time){
			av_frame_free(&newFrame.avFrame);
			
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
		double pts = frameQueue.top().pts;

		// If the next frame is more than one second into the future or the past, 
		// set the time to now

		if(pts > time + 1.0 || pts < time - 1.0){
			FlogD("adjusted time");
			timeHandler->SetTime(pts);
		}
	}
	
	int fetchAudio(int16_t* data, int nSamples)
	{
		return audioHandler->fetchAudio(data, nSamples);
	}

	bool update(double deltaTime)
	{
		addTime(deltaTime);
		tick();
		adjustTime();
		Frame newFrame = fetchFrame();

		if(newFrame.avFrame){
			// Don't free currentFrame if it is currentFrame itself that's being converted
			if(currentFrame.avFrame != newFrame.avFrame){
				av_frame_free(&currentFrame.avFrame);
				currentFrame = newFrame; // Save the current frame for snapshots etc.
			}
		
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
			sws_scale(swsCtx, (uint8_t**)currentFrame.avFrame->data, currentFrame.avFrame->linesize, 0, pCodecCtx->height, pict.data, pict.linesize); 
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
				lastError = ESeeking;
				FlogD("seek failed");
				return false;
			}else{
				FlogD("used seekRaw");
			}
		}else{
			FlogD("used seekTs");
		}

		timeHandler->SetTime(lastPts);
		stepIntoQueue = true;

		reachedEof = 0;
		reportedEof = false;

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
		while(!IsEof() && !success){
			try {
				while(frameQueue.size() < (unsigned int)frameQueueSize || 
						(hasAudioStream() && audioHandler->getAudioQueueSize() < audioDevice->GetBlockSize())){
					decodeFrame(true);
				}

				success = true;
				reachedEof = 0;
				reportedEof = false;
			}

			catch(std::runtime_error e){
				reachedEof++;
				if(IsEof()){
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
		double pts = 0;

		if(frameQueue.GetContainer().size() > 0)
			pts = frameQueue.GetContainer()[0].pts;

		return pts - timeFromPts(firstPts);
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

	Frame getCurrentFrame(){
		return currentFrame;
	}

	void setPlaybackSpeed(double speed){
		timeHandler->SetTimeWarp(speed);
	}

	FrameType decodeFrame(bool addToQueue, bool demux = true){
		int frameFinished = 0;
		int panicFrame = 0;
		FrameType ret = TAudio;

		//av_free(decFrame->data[0]);

		try {
			while(!frameFinished){
				int panic = 0; // A "panic" counter for breaking out of potentially infinite while loops (TODO how 'bout a for loop?)
				if(panicFrame++ > 1024){
					lastError = EDecoding;
					throw std::runtime_error("panic in frame loop");
				}

				// Fetch and decode until a frame is decoded
				if(demux){
					do{
						// Read frames until we get a video frame
						freePacket();
						int ret = 0;
						if((ret = av_read_frame(pFormatCtx, &packet)) < 0/* || panic++ > 1024*/){
							freePacket();
							lastError = EDemuxing;
							//FlogExpD(ret);
							throw std::runtime_error("demuxing");
						}
					}while(packet.stream_index != videoStream && packet.stream_index != audioStream);
				}

				demux = true;

				int bytesRemaining = packet.size;
				int bytesDecoded = 0;
				panic = 0;

				// Decode until all bytes in the read frame is decoded
				while(bytesRemaining > 0){
					if(packet.stream_index == videoStream){
						// Decode video

						//FlogExpD(packet.flags & PKT_FLAG_KEY);

						if( (bytesDecoded = avcodec_decode_video2(pCodecCtx, decFrame, &frameFinished, &packet)) < 0 )
						{
							lastError = EDecoding;
							throw std::runtime_error("video");
						}

						lastDts = packet.dts;

						ret = TVideo;
					}else{
						if((bytesDecoded = audioHandler->decode(packet, pFormatCtx->streams[audioStream], timeHandler->GetTimeWarp(), addToQueue)) <= 0){
							bytesDecoded = bytesRemaining; // skip audio
						}
					}
				
					if(panic++ > 1024){
						lastError = EDecoding;
						throw std::runtime_error("panic");
					}

					bytesRemaining -= bytesDecoded;
				}
			}
		}

		catch (std::runtime_error e) 
		{
			FlogD("Error decoding: \"" << e.what() << "\"");
			if((std::string)e.what() != "audio"){
				FlogD("re-throwing");
				throw(e);
			}
		}

		int64_t pts = av_frame_get_best_effort_timestamp(decFrame);
		this->lastPts = timeFromPts(av_frame_get_best_effort_timestamp(decFrame));

		if(firstPts == 0 || pts < firstPts)
			firstPts = pts;

		if(addToQueue && ret == TVideo){
			AVFrame* clone = cloneFrame(decFrame);
			frameQueue.push(Frame(clone, this->lastPts));
		}

		return ret;
	}

	bool decodeVideoFrame(){
		for(int i = 0; i < 100; i++){
			try{
				if(decodeFrame(false) == TVideo){
					return true;
				}
			}
			catch(std::runtime_error e){
				FlogD("generated exception");
				FlogExpD(e.what());
				if((std::string)e.what() == "demuxing"){
					return false;
				}
			}
		}

		FlogD("couldn't find a video frame in 100 steps");
		return false;
	}

	AVFrame* cloneFrame(AVFrame* src){
		AVFrame* ret = av_frame_alloc();
		uint8_t *buffer = (uint8_t *)av_malloc(avpicture_get_size((AVPixelFormat)src->format, src->width, src->height));

		if(!buffer || !ret){
			if(ret)
				av_frame_free(&ret);

			if(buffer)
				av_free(buffer);

			throw std::runtime_error("allocation failed in cloneframe");
		}

		avpicture_fill((AVPicture *) ret, buffer, pCodecCtx->pix_fmt, src->width, src->height);
		av_picture_copy((AVPicture*)ret, (AVPicture*)decFrame, pCodecCtx->pix_fmt, src->width, src->height);
		
		return ret;
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
		// Free buffered decoded frames
		FlogExpD("emptying framequeue");
		while(!frameQueue.empty()){
			Frame topFrame = frameQueue.top();
			frameQueue.pop();
			av_frame_free(&topFrame.avFrame);
		}
	}

	bool hasAudioStream()
	{
		return audioStream != AVERROR_STREAM_NOT_FOUND && audioStream != AVERROR_DECODER_NOT_FOUND;
	}

	bool openFile(StreamPtr stream, IAudioDevicePtr audioDevice){
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
			lastError = EFile;
			return false;
		}

		/* Get stream information */
		if(avformat_find_stream_info(pFormatCtx, NULL) < 0){
			FlogE("couldn't get stream info");
			lastError = EStreamInfo;
			return false;
		}

		/* Print video format information */
		av_dump_format(pFormatCtx, 0, stream->GetPath().c_str(), 0);

		// If the loader logged something about wmv being DRM protected, give up
		if(drm){
			lastError = EStream;
			return false;
		}

		// find the best audio and video streams
		audioStream = videoStream = AVERROR_STREAM_NOT_FOUND;
		pCodec = 0;

		videoStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);

		if(videoStream == AVERROR_STREAM_NOT_FOUND){
			FlogE("couldn't find stream");
			lastError = EStream;
			return false;
		}	
		
		if(videoStream == AVERROR_DECODER_NOT_FOUND || !pCodec){
			FlogE("unsupported codec");
			lastError = EVideoCodec;
			return false;
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
			lastError = EVideoCodec;
			return false;
		}

		w = pCodecCtx->width;
		h = pCodecCtx->height;

		FlogExpD(pFormatCtx->streams[videoStream]->r_frame_rate.num);
		FlogExpD(pFormatCtx->streams[videoStream]->r_frame_rate.den);
		FlogExpD(pFormatCtx->duration / AV_TIME_BASE);

		return true;
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

		errorCallback(EUnloadedFile, stream->GetPath());
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

	void addTime(double t){
		timeHandler->AddTime(t);
	}
	
	double getTime(){
		return timeHandler->GetTime();
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

VideoPtr Video::Create(StreamPtr stream, ErrorCallback errorCallback, IAudioDevicePtr audioDevice, int frameQueueSize)
{
	static bool initialized = false;
	if(!initialized)
		av_register_all();

	CVideo* video = new CVideo(errorCallback, frameQueueSize);

	av_log_set_callback(logCb);
	av_log_set_level(AV_LOG_WARNING);

	if(!video->openFile(stream, audioDevice)){
		errorCallback(EFile, "could not open file");
		delete video;
		return 0;
	}

	return VideoPtr(video);
}
