#include <vector>
#include <string>

#include "VideoException.h"

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
