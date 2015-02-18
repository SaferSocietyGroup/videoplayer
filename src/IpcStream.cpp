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

#include <lfsc.h>

#include "IpcStream.h"
#include "Tools.h"
#include "Flog.h"

class CIpcStream : public IpcStream
{
	public:
	lfsc_file* f = 0;
	AVIOContext* ctx = 0;
	std::wstring filename;
	unsigned char* buffer;
	
	~CIpcStream()
	{
		Close();
	}

	void Open(const std::wstring& filename, struct lfsc_file* file)
	{
		this->f = file;
		this->filename = filename;

		int flags = 0;
		lfsc_status s = lfsc_get_flags(f, &flags);

		if(s != LFSC_SOK)
			throw StreamEx("could not get file flags");
		
		int size = FF_INPUT_BUFFER_PADDING_SIZE + 1024 * 32;
		buffer = (unsigned char*)av_mallocz(size);
		if(buffer == NULL)
			throw StreamEx("failed to allocate RAM");

		ctx = GenAVIOContext(buffer, size, /*(flags & LFSC_SERR_WRITE_PIPE) ? true : false*/ false);

		if(ctx == NULL)
			throw StreamEx("failed to allocate RAM");
	}
	
	int Read(uint8_t *buf, int buf_size)
	{
		return lfsc_read(buf, buf_size, f);
	}

	int Write(uint8_t *buf, int buf_size)
	{
		return lfsc_write(buf, buf_size, f);
	}

	int64_t Seek(int64_t offset, int whence)
	{
		if(whence == AVSEEK_SIZE)
			return lfsc_get_length(f); 

		return lfsc_fseek(f, offset, whence & 3);
	}

	std::string GetPath()
	{
		return Tools::WstrToStr(filename);
	}

	AVIOContext* GetAVIOContext()
	{
		return ctx;
	}

	void Close()
	{
		if(f)
			lfsc_fclose(f);
			
		f = 0;
	}
};

IpcStreamPtr IpcStream::Create()
{
	return std::make_shared<CIpcStream>();
}
