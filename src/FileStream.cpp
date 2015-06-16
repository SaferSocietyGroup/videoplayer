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

#include <cstdio>
#include "FileStream.h"
#include "Flog.h"
#include "Tools.h"

class CFileStream : public FileStream
{
	public:
	FILE* f = 0;
	std::string filename;
	AVIOContext* ctx = 0;
	unsigned char* buffer = 0;
	long fileSize;
	
	void Open(const std::string& filename, bool rw)
	{
		std::string mode = rw ? "rb" : "rwb";
		f = fopen(filename.c_str(), mode.c_str());

		FlogExpD(filename);

		if(!f)
			throw StreamEx(Str("could not open file: " << filename));
		
		fseek(f, 0, SEEK_END);
		fileSize = ftell(f);
		fseek(f, 0, SEEK_SET);

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
		if(whence == AVSEEK_SIZE)
			return fileSize;

		return fseek(f, offset, whence & 3);
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
