#ifndef VIDEO_EXCEPTION_H
#define VIDEO_EXCEPTION_H

#include <string>

struct VideoException : public std::exception
{
	enum ErrorCode {
		EFile,
		EVideoCodec,
		EStreamInfo,
		EStream,
		EDemuxing,
		EDecodingVideo,
		EDecodingAudio,
		ESeeking,
		EScaling,
		ERetries,
	};

	ErrorCode errorCode;

	std::string what();
	VideoException(ErrorCode errorCode);
};

#endif
