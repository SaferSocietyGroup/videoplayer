#include <cstdio>
#include "filestream.h"
#include "flog.h"
#include "tools.h"

class CFileStream : public FileStream
{
	public:
	FILE* f = 0;
	std::string filename;
	AVIOContext* ctx = 0;
	unsigned char* buffer = 0;
	
	void Open(const std::string& filename, bool rw)
	{
		std::string mode = rw ? "rb" : "rwb";
		f = fopen(filename.c_str(), mode.c_str());

		if(!f)
			throw StreamEx(Str("could not open file: " << filename));

		this->filename = filename;
		int size = FF_INPUT_BUFFER_PADDING_SIZE + 1024 * 32;

		buffer = (unsigned char*)av_mallocz(size);

		if(buffer == NULL)
			throw StreamEx("failed to allocate RAM");

		ctx = GenAVIOContext(buffer, size, rw);

		if(ctx == NULL)
			throw StreamEx("failed to allocate RAM");
	}
	
	std::string GetPath()
	{
		return filename;
	}
	
	int Read(uint8_t *buf, int buf_size)
	{
		return fread((void*)buf, 1, buf_size, f);
	}

	int Write(uint8_t *buf, int buf_size)
	{
		return fwrite(buf, 1, buf_size, f);
	}

	int64_t Seek(int64_t offset, int whence)
	{
		return fseek(f, offset, whence);
	}

	AVIOContext* GetAVIOContext()
	{
		return ctx;
	}
	
	void Close()
	{
		if(ctx){
			avio_flush(ctx);

			// TODO leaking memory, but if it's enabled ffmpeg crashes.
			// av_freep(&buffer);

			av_free(ctx);

			ctx = NULL;
			buffer = NULL;
		}
		
		if(f){
			fclose(f);
			f = NULL;
		}
	}

	~CFileStream()
	{
		Close();
	}
};

FileStreamPtr FileStream::Create()
{
	return std::make_shared<CFileStream>();
}
