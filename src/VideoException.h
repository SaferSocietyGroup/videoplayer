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
		ERetries,
	};

	ErrorCode errorCode;

	std::string what();
	VideoException(ErrorCode errorCode);
};
