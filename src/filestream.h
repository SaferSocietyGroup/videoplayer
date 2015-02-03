#ifndef FILESTREAM_H
#define FILESTREAM_H

#include <memory>

#include "stream.h"

typedef std::shared_ptr<class FileStream> FileStreamPtr;

class FileStream : public Stream
{
	virtual void Open(const std::string& filename, bool rw = false) = 0;
	static FileStreamPtr Create();
};

#endif
