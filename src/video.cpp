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

#include "video.h"
#include "font.h"
#include "log.h"
#include "bitmap.h"
#include "flog.h"

class KfPos
{
	public:
	KfPos(int f = 0, int64_t p = 0, int64_t d = 0){frame = f; pos = p; dts = d;}
	int frame;
	int64_t pos;
	int64_t dts;
};

struct FrameSorter
{
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
	
	AudioHandlerPtr audioHandler;
	TimeHandler timeHandler;

	AVFormatContext *pFormatCtx;
	int audioStream, videoStream;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;
	
	AVPicture pict;
	AVPacket packet;

	int duration;
	int framePosition;
	int reachedEof;
	int frameQueueSize;

	Bitmap frameBitmap;
	Error lastError;

	std::priority_queue<Frame, std::vector<Frame>, FrameSorter> frameQueue;
	
	double lastPts;
	int64_t lastDts;
	int64_t firstPts;

	AVFrame* decFrame;

	Frame currentFrame;
	std::vector<KfPos> keyframes;
	std::string filename;
	float t;
	bool drawTimeStamp;
	bool reportedEof;
	
	CVideo(ErrorCallback errorCallback, int frameQueueSize){
		this->errorCallback = errorCallback;

		this->frameQueueSize = frameQueueSize;
		reachedEof = 0;
		framePosition = 0;
		pCodec = 0;
		pCodecCtx = 0;
		pFormatCtx = 0;
		lastError = ENoError;
		drawTimeStamp = true;
		duration = -1;
		memset(&packet, 0, sizeof(AVPacket));
		lastPts = 0;
		lastDts = 0;
		firstPts = 0;
		t = 0.0f;
		stepIntoQueue = true;
		reportedEof = false;

		timeHandler.pause();
		decFrame = avcodec_alloc_frame();
	}

	~CVideo(){
		LogDebug("destructor");

		closeFile();

		emptyFrameQueue();

		av_free(decFrame);
		////freeFrame(decFrame);
		freeFrame(&currentFrame.avFrame);
	}

	Frame fetchFrame(){
		if(frameQueue.empty() && IsEof() && !reportedEof){
			reportedEof = true;
			errorCallback(EEof, "eof");
		}

		if(stepIntoQueue && !frameQueue.empty()){
			stepIntoQueue = false;
			timeHandler.setTime(frameQueue.top().pts + .001);
		}

		double time = timeHandler.getTime();

		Frame newFrame;

		// Throw away all old frames (timestamp older than now) except for the last
		// and set the pFrame pointer to that.

		while(!frameQueue.empty() && frameQueue.top().pts < time){
			freeFrame(&newFrame.avFrame);

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

		double time = timeHandler.getTime();
		double pts = frameQueue.top().pts;

		// If the next frame is more than one second into the future or the past, 
		// set the time to now

		if(pts > time + 1.0 || pts < time - 1.0){
			FlogD("adjusted time");
			//timeHandler.setTime(pts);
		}
	}

	/* Decode frame, convert to RGB and put on given surface with given dimensions */
	/* Note: this method must be called on each fetched frame or the decoding won't progress */
	/* TODO: this function has tons of side effects and really shouldn't work like it does */
	void frameToSurface(Frame newFrame, uint8_t* buffer, int w, int h, int sw, int sh){
		if(!w) w = pCodecCtx->width; 
		if(!h) h = pCodecCtx->height; 
		if(!sw) sw = w;
		if(!sh) sh = h;

		if(!newFrame.avFrame)
			return;

		// Don't free currentFrame if it is currentFrame itself that's being converted
		if(currentFrame.avFrame != newFrame.avFrame){
			freeFrame(&currentFrame.avFrame);
			currentFrame = newFrame; // Save the current frame for snapshots etc.
		}

		Bitmap bmp(sw, sh, (void*)buffer, 3);
		frameToBitmap(currentFrame.avFrame, bmp, sw, sh); 

		if(drawTimeStamp){
			//double t = (int64_t)currentFrame.pts - timeFromPts(this->firstPts);
			double t = timeHandler.getTime();

			int seconds = (int)t % 60;
			int hours = (int)(t / 3600);
			int minutes = (int)(t / 60) - (hours * 60);
			int frame = CLAMP(0, 5000, (int)((double)getFrameRate() * fmod(t, 1)));

			char buffer[512];
			snprintf(buffer, sizeof(buffer), "%d:%02d:%02d.%02d", hours, minutes, seconds, frame);

			Bitmap tt = DrawText(
					buffer, 
					Color(255, 255, 255, 255));

			tt.draw(bmp, bmp.w - tt.w - 10, 10);
			tt.freePixels();
		}
	}

	/* Seek to given frame */
	bool seek(int frame, bool exact = false){
		reachedEof = 0;
		reportedEof = false;
		frame = std::min(std::max(0, frame), getDurationInFrames());
		emptyFrameQueue();

		LogExp(duration);
		FlogD("Seeking to " << frame << " of " << getDurationInFrames());

		int to = frame;

		// Try to seek with keyframe table
		if(!seekKf(to)){
			// Try to seek with timestamp

			if(exact){
				to = std::max(frame - 100, 0);
			}

			if(!seekTs(to, exact /* seek to any frame */)){
				// Try to seek with raw byte seeking
				if(!seekRaw(to)){
					lastError = ESeeking;
					LogDebug("seek failed");
					return false;
				}else{
					LogDebug("used seekRaw");
				}
			}else{
				LogDebug("used seekTs");
			}

			if(exact){
				// Step to the requested frame
				try {
					// if it can't get to the requested frame in 350 tries, bail
					for(int i = 0; i < 350; i++){
						decodeFrame(false);
						if(getStreamPosition() >= frame)
							break;
					}
				}

				catch(std::runtime_error e){
					LogDebug("Error decoding: " << e.what());
					return false;
				}
			}
		}else{
			LogDebug("used seekKf");
		}

		timeHandler.setTime(lastPts);
		stepIntoQueue = true;

		reachedEof = 0;
		reportedEof = false;

		return true;
	}

	/* Step to next frame */
	bool step(){
		float t = timeHandler.getTime(), fps = getFrameRate();
		timeHandler.setTime((t * fps + 1.0) / fps);
		return true;
	}

	bool stepBack(){
		float t = timeHandler.getTime(), fps = getFrameRate();
		return seek(floorf(t * fps - 2.0), true);
	}

	void tick(){
		bool success = false;
		while(!IsEof() && !success){
			try {
				while(frameQueue.size() < (unsigned int)frameQueueSize){
					decodeFrame(true);
				}

				/*while(!frameQueue.empty()){
					FlogExpD(frameQueue.top().pts);
					frameQueue.pop();
				}*/

				success = true;
				reachedEof = 0;
				reportedEof = false;
			}

			catch(std::runtime_error e){
				reachedEof++;
				if(IsEof()){
					FlogExpD(reachedEof);
				}
				//}else{
				//	FlogD("demuxer failed, trying again");
				//}
			}
		}
	}
	
	void play(){
		if(IsEof())
			errorCallback(EEof, "eof");
		else
			timeHandler.play();
	}

	int getWidth(){
		return w;
	}

	int getHeight(){
		return h;
	}

	int getDurationInFrames(){
		if(duration > -1){
			/* if a duration was calculated (in locateKeyFrames), use it */
			return duration;
		}

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

	int getPosition(){
		if(!frameQueue.empty()){
			return currentFrame.frame;
		}

		return getStreamPosition();
	}

	int getStreamPosition(){
		// if the stream's timestamp is invalid, calculate the position from the frame rate
		if(!isValidTs((uint64_t)pFormatCtx->streams[videoStream]->cur_dts)){
			return (int)((lastPts - timeFromPts(firstPts)) * getFrameRate());
		}

		// the stream's timestamp is valid, calculate frame position based on that
		return (int)(timeFromPts(pFormatCtx->streams[videoStream]->cur_dts - firstPts) 
				* getFrameRate());
	}

	void freePacket(){
		av_free_packet(&packet);
		memset(&packet, 0, sizeof(AVPacket));
	}

	float getAspect(){
		return ((float)w * getPAR()) / h;
	}

	std::string getIFrames(){
		std::stringstream s;

		if(duration == -1){
			locateKeyFrames();
		}

		s << duration << " ";

		for(std::vector<KfPos>::iterator it = keyframes.begin(); it != keyframes.end(); it++){
			s << (*it).frame << " " << (*it).pos << " " << (*it).dts << " ";
		}

		return s.str();
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
	
	int getVideoBitRate(){
		return pCodecCtx->bit_rate;
	}

	static bool drm;

	Frame getCurrentFrame(){
		return currentFrame;
	}

	void setIFrames(std::string iframes){
		keyframes.clear();
		std::stringstream s(iframes);

		s >> duration;

		while(true){
			int frame;
			int64_t dts, pos;

			s >> frame;
			s >> pos;
			s >> dts;

			if(!s.good()){
				break;
			}
			keyframes.push_back(KfPos(frame, pos, dts));
		}
		LogDebug("loaded " << keyframes.size() << " iframes");
	}

	void setPlaybackSpeed(double speed){
		timeHandler.setTimeWarp(speed);
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
							//LogExp(ret);
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

						//LogExp(packet.flags & PKT_FLAG_KEY);

						if( (bytesDecoded = avcodec_decode_video2(pCodecCtx, decFrame, &frameFinished, &packet)) < 0 )
						{
							lastError = EDecoding;
							throw std::runtime_error("video");
						}

						lastDts = packet.dts;
						framePosition++;

						ret = TVideo;
					}else{
						if((bytesDecoded = audioHandler->decode(packet, pFormatCtx->streams[audioStream],timeHandler.getTimeWarp())) <= 0){
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
			LogDebug("Error decoding: \"" << e.what() << "\"");
			if((std::string)e.what() != "audio"){
				LogDebug("re-throwing");
				throw(e);
			}
		}

		int64_t pts = av_frame_get_best_effort_timestamp(decFrame);
		this->lastPts = timeFromPts(av_frame_get_best_effort_timestamp(decFrame));

		if(firstPts == 0 || pts < firstPts)
			firstPts = pts;

		if(addToQueue && ret == TVideo){
			AVFrame* clone = cloneFrame(decFrame);
			frameQueue.push(Frame(clone, this->lastPts, framePosition));
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
				LogDebug("generated exception");
				LogExp(e.what());
				if((std::string)e.what() == "demuxing"){
					return false;
				}
			}
		}

		LogDebug("couldn't find a video frame in 100 steps");
		return false;
	}


	void frameToBitmap(AVFrame* pFrame, Bitmap& bitmap, int w, int h){
		PixelFormat fffmt = bitmap.bytesPerPixel == 4 ? PIX_FMT_BGRA : PIX_FMT_BGR24;

		int avret = avpicture_fill(&pict, (uint8_t*)bitmap.pixels , fffmt, bitmap.w, bitmap.h);

		if(avret < 0){
			LogError("avpicture_fill returned " << avret);
			throw std::runtime_error("avpicture_fill error");
		}

		struct SwsContext* swsCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, 
			pCodecCtx->pix_fmt, w, h, fffmt, SWS_BILINEAR, NULL, NULL, NULL);

		if(swsCtx){
			sws_scale(swsCtx, (uint8_t**)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pict.data, pict.linesize); 
			sws_freeContext(swsCtx);
		}else{
			LogError("Failed to get a scaling context");
		}
		
		// TODO why doesn't it set the alpha channel automatically?

		if(bitmap.bytesPerPixel == 4){
			Color* p = (Color*)bitmap.pixels;
			for(int i = 0; i < bitmap.w * bitmap.h; i++)
				(p++)->a = 255;
		}
	}

	AVFrame* cloneFrame(AVFrame* src){
		AVFrame* ret = avcodec_alloc_frame();

		if(!ret){
			throw std::runtime_error("allocation failed in cloneframe");
		}
		uint8_t *buffer = (uint8_t *)av_malloc(avpicture_get_size(pCodecCtx->pix_fmt, src->width, src->height) * 2);
		/*uint8_t *buffer = (uint8_t *)av_malloc(avpicture_get_size(src->format, src->width, src->height));*/

		avpicture_fill((AVPicture *) ret, buffer, pCodecCtx->pix_fmt, src->width, src->height);
		av_picture_copy((AVPicture*)ret, (AVPicture*)decFrame, pCodecCtx->pix_fmt, src->width, src->height);
		
		//printf("%p vs. %p\n", buffer, ret->data[0]);
		return ret;
	}

	void freeFrame(AVFrame** frame){
		if(*frame){
			av_free((*frame)->data[0]);
			av_free(*frame);
			*frame = 0;
		}
	}

	// Raw byte seeking
	bool seekRaw(int frame){
		double to = (double)frame / (double)getDurationInFrames();
		int64_t fileSize = avio_size(pFormatCtx->pb);
		if(av_seek_frame(pFormatCtx, -1, (int64_t)((double)fileSize * to), AVSEEK_FLAG_BYTE) < 0){
			LogError("Raw seeking error");
			return false;
		}

		avcodec_flush_buffers(pCodecCtx);
		decodeVideoFrame();
		framePosition = getStreamPosition();
		return true;
	}

	// Seek by timestamp
	bool seekTs(int frame, bool any){
		int seekFlags = 0;

		//FlogExpD(frame);

		if(any){
			seekFlags = AVSEEK_FLAG_ANY;
		}

		if(frame > getStreamPosition()){
			seekFlags = seekFlags | AVSEEK_FLAG_BACKWARD;		
		}

		double ts = (double)frame / getFrameRate() * (double)AV_TIME_BASE;

		//FlogExpD((int64_t)ts);
		//FlogExpD((int64_t)ts + firstPts);

		int seekRet = av_seek_frame(pFormatCtx, -1, (int64_t)ts + firstPts, seekFlags);
		if(seekRet > 0){
			FlogD("av_seek_frame failed, returned " << seekRet);
			return false;
		}

		avcodec_flush_buffers(pCodecCtx);

		bool ret = decodeVideoFrame();
		framePosition = getStreamPosition();

		if(abs(frame - framePosition) > 25)
			return false;

		return ret;
	}

	// Seek by keyframe table (requires a keyframe table generated by locateKeyFrames())
	bool seekKf(int frame){
		// Check if we have a keyframe table
		if(!keyframes.size()){
			return false;
		}

		int at = testSeekKf(frame, true);

		if(at < 0){
			return false;
		}

		// Decode the keyframe
		if(!decodeVideoFrame()){
			return false;
		}

		// Decode frames until we get to the frame we really want to
		for(int i = 0; i < frame - at; i++){
			if(!decodeVideoFrame()){
				return false;
			}
		}

		// We should be at the correct frame, set our position to it
		framePosition = frame;
		return true;
	}


	void emptyFrameQueue(){
		// Free buffered decoded frames
		while(!frameQueue.empty()){
			Frame topFrame = frameQueue.top();
			frameQueue.pop();
			freeFrame(&topFrame.avFrame);
		}
	}

	void locateKeyFrames(){
		LogDebug("Locating keyframes");

		duration = 0;
		keyframes.clear();

		int eofCounter = 0;

		for(;;)
		{
			int64_t pos = avio_seek(pFormatCtx->pb, 0, SEEK_CUR);
			/*LogDebug("  ");
				LogExp(url_ftell(pFormatCtx->pb));*/
			int ret = av_read_frame(pFormatCtx, &packet);

			// might not be finished even if the file has reached the end, add to a counter
			if(pFormatCtx->pb->eof_reached)
				eofCounter++;

			// Ret SHOULD return < 0 when eof is reached, but it doesn't always
			// if it doesn't, check the file pointer if it has eof set for the last 1000 loops
			// then we could probably safely assume that there are no more packets to fetch

			if(ret < 0 || eofCounter > 100){
				freePacket();
				break;
			}

			if(packet.stream_index == videoStream){
				if(packet.flags & AV_PKT_FLAG_KEY){
					bool ins = true;

					// TODO optimize
					for(std::vector<KfPos>::iterator it = keyframes.begin(); it != keyframes.end(); it++)
					{
						if((*it).pos == pos || (*it).dts == packet.dts)
						{
							ins = false;
							break;
						}
					}

					if(ins){
						keyframes.push_back(KfPos(duration, pos, packet.dts));
					}else{
						LogDebug("skipping duplicate");
					}
				}

				duration++;
				freePacket();
			}
		}

		FlogExpD(duration);

		FlogD("Removing Unseekable Keyframes");


		// Remove any non working keyframe positions (WMV crap)
		std::vector<KfPos>::iterator it = keyframes.begin();
		int removed = 0;

		while (it != keyframes.end())
		{
			/* Try to seek to keyframe and decode a videoframe */
			if(testSeekKf((*it).frame, false) != -1/* && decodeVideoFrame()*/){
				//LogDebug("kf " << (*it).frame << " seekable, keeping");
				++it;
			}else{
				LogDebug("kf unseekable, removing from list");
				it = keyframes.erase(it);
				removed++;
			}
		}

		if(keyframes.size() == 0){
			keyframes.push_back(KfPos(0, 0, 0));
		}

		LogExp(removed);
	}

	int testSeekKf(int frame, bool adjust = true){
		KfPos closeKf;
		KfPos lastKf;

		// Locate the keyframe closest to the frame we want

		for(std::vector<KfPos>::iterator it = keyframes.begin(); it != keyframes.end(); it++){
			if((*it).frame > frame){
				break;
			}
			lastKf = closeKf;
			closeKf = *it;
		}

		//LogDebug("kf: " << closeKf.pos << " " << closeKf.frame << " " << closeKf.dts);

		int ret = av_seek_frame(pFormatCtx, videoStream, closeKf.dts, AVSEEK_FLAG_ANY);
		avcodec_flush_buffers(pCodecCtx);

		if(ret < 0){
			//LogDebug("dts seek failed, trying url_fseek");

			// Use the previous keyframe
			if(adjust && closeKf.frame != 0){ closeKf = lastKf; }

			avio_seek(pFormatCtx->pb, closeKf.pos, SEEK_SET);
			avcodec_flush_buffers(pCodecCtx);
		}else{
			return closeKf.frame;
		}

		for(int i = 0; i < 1000; i++){
			ret = av_read_frame(pFormatCtx, &packet);
			if(ret < 0){
				LogDebug("read frame failed");
				freePacket();
				return -1;
			}

			if(packet.flags & AV_PKT_FLAG_KEY){
				//LogDebug("Found kf after " << i << " steps");
				return closeKf.frame;
			}

			freePacket();
		}

		LogDebug("Couldn't find kf after 1000 steps, failing");
		return -1;
	}

	bool openFile(AudioCallback audioCallback, int freq, int channels){
		LogInfo("Trying to load file: " << filename);

		int ret;

		pFormatCtx = NULL;
		if((ret = avformat_open_input(&pFormatCtx, filename.c_str(), NULL, NULL)) != 0){
			char ebuf[512];
			av_strerror(ret, ebuf, sizeof(ebuf));
			LogError("couldn't open file");
			LogError(ebuf);
			lastError = EFile;
			return false;
		}

		/* Get stream information */
		if(avformat_find_stream_info(pFormatCtx, NULL) < 0){
			LogError("couldn't get stream info");
			lastError = EStreamInfo;
			return false;
		}

		/* Print video format information */
		av_dump_format(pFormatCtx, 0, filename.c_str(), 0);

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
			LogError("couldn't find stream");
			lastError = EStream;
			return false;
		}	
		
		if(videoStream == AVERROR_DECODER_NOT_FOUND || !pCodec){
			LogError("unsupported codec");
			lastError = EVideoCodec;
			return false;
		}

		audioStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

		if(audioStream != AVERROR_STREAM_NOT_FOUND && audioStream != AVERROR_DECODER_NOT_FOUND){
			audioHandler = AudioHandler::Create(pFormatCtx->streams[audioStream]->codec, audioCallback, freq, channels);
		}else{
			LogDebug("no audio stream or unsupported audio codec");
		}
		
		/* Get a pointer to the codec context for the video stream */
		pCodecCtx = pFormatCtx->streams[videoStream]->codec;

		// Open codec
		if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0){
			LogError("unsupported codec");
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

		if(pFormatCtx)
			av_close_input_file(pFormatCtx);

		errorCallback(EUnloadedFile, filename);
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
		timeHandler.pause();
	}

	bool getPaused(){
		return timeHandler.getPaused();
	}

	void addTime(double t){
		timeHandler.addTime(t);
	}
	
	double getTime(){
		return timeHandler.getTime();
	}
};

bool CVideo::drm = false;

static void logCb(void *ptr, int level, const char *fmt, va_list vargs)
{
	if(level == AV_LOG_WARNING){

		/* HACK, can we extract this information from the headers structures somehow? */

		if(!strcmp(fmt, "DRM protected stream detected, decoding will likely fail!\n")){
			LogInfo("DRM protected stream");
			CVideo::drm = true;
		}

		else if(!strcmp(fmt, "Ext DRM protected stream detected, decoding will likely fail!\n")){
			LogInfo("Ext DRM protected stream");
			CVideo::drm = true;
		}

		else if(!strcmp(fmt, "Digital signature detected, decoding will likely fail!\n")){
			LogInfo("Digitally signed stream");
			CVideo::drm = true;
		}
	}

	if (level <= av_log_get_level()){
		char tmp[1024];
		vsprintf(tmp, fmt, vargs);

		LogDebug("ffmpeg says: " << tmp);
	}
}

VideoPtr Video::Create(const std::string& filename, ErrorCallback errorCallback, AudioCallback audioCallback,
	int freq, int channels, int frameQueueSize, const std::string& kf)
{
	static bool initialized = false;
	if(!initialized)
		av_register_all();

	CVideo* video = new CVideo(errorCallback, frameQueueSize);
	video->filename = filename;

	video->setIFrames(kf);

	av_log_set_callback(logCb);
	av_log_set_level(AV_LOG_WARNING);

	if(!video->openFile(audioCallback, channels, freq)){
		errorCallback(EFile, "could not open file");
		delete video;
		return 0;
	}

	return VideoPtr(video);
}

Frame::Frame(AVFrame* f, double pts, int fn)
{
	avFrame = f;
	this->pts = pts;
	frame = fn;
}

