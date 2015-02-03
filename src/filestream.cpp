#include <fstream>
#include "filestream.h"

class CFileStream : public FileStream
{
	public:
	std::fstream stream;
	std::string filename;
	AVIOContext* ctx;
	unsigned char* buffer = 0;
	
	void Open(const std::string& filename, bool rw)
	{
		stream.open(filename, std::ios_base::binary | std::ios_base::in | (rw ? std::ios_base::out : std::ios_base::in));
		this->filename = filename;
		int size = FF_INPUT_BUFFER_PADDING_SIZE + 1024 * 1024;
		buffer = (unsigned char*)av_mallocz(size);
		ctx = GenAVIOContext(buffer, size, rw);
	}
	
	std::string GetPath()
	{
		return filename;
	}
	
	int Read(uint8_t *buf, int buf_size)
	{
		stream.read((char*)buf, buf_size);
		return stream.gcount();
	}

	int Write(uint8_t *buf, int buf_size)
	{
		stream.write((char*)buf, buf_size);
		return stream.good() ? buf_size : 0;
	}

	int64_t Seek(int64_t offset, int whence)
	{
		stream.seekg(offset, (std::ios_base::seekdir)whence);
		return stream.tellg();
	}

	AVIOContext* GetAVIOContext()
	{
		return ctx;
	}

	~CFileStream()
	{
		if(stream.is_open())
			stream.close();

		if(buffer)
			av_free(buffer);

		// todo free context
	}
};

FileStreamPtr FileStream::Create()
{
	return std::make_shared<CFileStream>();
}
