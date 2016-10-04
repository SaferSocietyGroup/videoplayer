#include "Stream.h"

int Stream::FFReadPacket(void *opaque, uint8_t *buf, int buf_size)
{
	return ((Stream*)opaque)->Read(buf, buf_size);
}

int Stream::FFWritePacket(void *opaque, uint8_t *buf, int buf_size)
{
	return ((Stream*)opaque)->Write(buf, buf_size);
}

int64_t Stream::FFSeek(void *opaque, int64_t offset, int whence)
{
	return ((Stream*)opaque)->Seek(offset, whence);
}

AVIOContext* Stream::GenAVIOContext(unsigned char* buffer, int bufferSize, bool rw)
{
	return avio_alloc_context(buffer, bufferSize, rw ? 1 : 0, this, FFReadPacket, FFWritePacket, FFSeek);
}
