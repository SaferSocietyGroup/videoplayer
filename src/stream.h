#ifndef STREAM_H
#define STREAM_H

#include <cstdint>
#include <string>
#include <libavformat/avformat.h>

class Stream
{
	public:
	// callbacks for FFMPEG
	static int FFReadPacket(void *opaque, uint8_t *buf, int buf_size);
	static int FFWritePacket(void *opaque, uint8_t *buf, int buf_size);
	static int64_t FFSeek(void *opaque, int64_t offset, int whence);

	// methods to override
	virtual int Read(uint8_t *buf, int buf_size) = 0;
	virtual int Write(uint8_t *buf, int buf_size) = 0;
	virtual int64_t Seek(int64_t offset, int whence) = 0;
	virtual std::string GetPath() = 0;
	virtual AVIOContext* GetAVIOContext() = 0;

	protected:
	AVIOContext* GenAVIOContext(unsigned char* buffer, int bufferSize, bool rw);
};

#endif
