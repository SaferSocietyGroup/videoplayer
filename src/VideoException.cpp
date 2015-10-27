#include <vector>
#include <string>

#include "VideoException.h"

std::string VideoException::what()
{
	if((int)errorCode < 0 || (int)errorCode > (int)ERetries){
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
		"exceeded maximum number of retries",
	};

	return eStr[(int)errorCode];
}

VideoException::VideoException(ErrorCode errorCode){
	this->errorCode = errorCode;
}
