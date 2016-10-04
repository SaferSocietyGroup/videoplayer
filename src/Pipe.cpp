#include "Pipe.h"
#include "Tools.h"
#include "Flog.h"

#include <iomanip>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class CPipe : public Pipe
{
	public:
	HANDLE pipe = INVALID_HANDLE_VALUE;
	std::wstring nameFix;

	void Open(const std::wstring& name, int msTimeout)
	{
		nameFix = LStr(L"\\\\.\\pipe\\" << name);

		if(!WaitNamedPipeW(nameFix.c_str(), msTimeout))
			throw PipeException(Str("could not wait for pipe, error code: " << GetLastError()));

		pipe = CreateFileW(nameFix.c_str(), GENERIC_READ | GENERIC_WRITE | FILE_SHARE_READ | FILE_SHARE_WRITE, 
			0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if(pipe == INVALID_HANDLE_VALUE)
			throw PipeException(Str("could not open pipe, error code: " << GetLastError()));
	}

	void CreatePipe(const std::wstring& name)
	{
		nameFix = LStr(L"\\\\.\\pipe\\" << name);

		pipe = CreateNamedPipeW(nameFix.c_str(), PIPE_ACCESS_DUPLEX, 
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			1, 32, 32, 100, NULL);

		if(pipe == INVALID_HANDLE_VALUE)
			throw PipeException(Str("could not create pipe, error code: " << GetLastError()));
	}

	void WaitForConnection(int msTimeout)
	{
		if(!ConnectNamedPipe(pipe, NULL)){
			DWORD ec = GetLastError();
			if(ec != ERROR_PIPE_CONNECTED){
				throw PipeException(Str("could not wait for connection, error code: " << ec));
			}
		}
	}

	// makes sure a buffer pointing to an int of arbitrary size is little endian
	static void FlipIntCopy(void* dst, const void* src, int size)
	{
		uint32_t a = 1;

		if(((char*)&a)[0] == 0){
			// big endian, flip
			for(int i = 0; i < size; i++){
				((uint8_t*)dst)[i] = ((uint8_t*)src)[size - 1 - i];
			}
		}

		else {
			// little endian, copy
			memcpy(dst, src, size);
		}
	}

	void Write(const char* buffer, size_t size)
	{
		DWORD bw;

		if(!WriteFile(pipe, buffer, size, &bw, NULL))
			throw PipeException(Str("could not write to pipe, error code: " << GetLastError()));
		
		if(bw != size)
			throw PipeException(Str("pipe write length mismatch, bytes written: " << bw << ", expected write length: " << size));
	}

	std::wstring DecodeUTF8(const char* buffer, int byteSize)
	{
		if(byteSize == 0)
			return L"";

		int requiredSize = MultiByteToWideChar(CP_UTF8, 0, buffer, byteSize, 0, 0);
		std::wstring s(requiredSize, L' ');
		int written = MultiByteToWideChar(CP_UTF8, 0, buffer, byteSize, &s[0], s.size());

		if(written == 0)
			throw PipeException("failed to decode UTF8");

		return s;
	}

	std::string EncodeUTF8(const std::wstring& str)
	{
		int byteSize = WideCharToMultiByte(CP_UTF8, 0, str.data(), str.size(), 0, 0, 0, 0);
		std::string s(byteSize, ' ');
		WideCharToMultiByte(CP_UTF8, 0, str.data(), str.size(), &s[0], s.size(), 0, 0);
		return s;
	}

	void Read(char* buffer, size_t size)
	{
		DWORD br;

		size_t total = 0;

		while(total < size)
		{
			if(!ReadFile(pipe, buffer + total, size - total, &br, NULL))
				throw PipeException(Str("could not read from pipe, error code: " << GetLastError()));

			total += br;
		}

		if(total != size)
			throw PipeException(Str("pipe read length mismatch, bytes read: " << br << ", expected write length: " << size));
	}

	void WriteInt(const void* v, int size)
	{
		char buffer[size];
		FlipIntCopy(buffer, v, size);
		Write(buffer, size);
	}

	void ReadInt(void* out_v, int size)
	{
		char buffer[size];
		Read(buffer, size);
		FlipIntCopy(out_v, buffer, size);
	}
	
	void WriteFloat(float val)
	{
		Write((char*)&val, sizeof(float));
	}

	void WriteDouble(double val)
	{
		Write((char*)&val, sizeof(double));
	}

	float ReadFloat()
	{
		float ret;
		Read((char*)&ret, sizeof(float));
		return ret;
	}

	double ReadDouble()
	{
		double ret;
		Read((char*)&ret, sizeof(double));
		return ret;
	}

	int EncodeLEB128(char* buffer, uint64_t val)
	{
		size_t i = 0;

		for(i = 0; i < sizeof(uint64_t) + 10; i++){
			buffer[i] = ((val >> (i * 7)) & 0x7f) | 0x80;
			
			// 9 because 10 (i + 1) is the last byte of a 64 bit encode and uint64_t can't hold 10 * 7 = 70 bits 
			if(i == 9 || val >> ((i + 1) * 7) == 0){
				buffer[i] &= 0x7f;
				break;
			}
		}

		return i + 1;
	}

	uint64_t DecodeLEB128(char* buffer, int size)
	{
		uint64_t val = 0;

		for(int i = 0; i < size; i++){
			val |= ((uint64_t)(buffer[i] & 0x7f)) << (7 * i);

			if((uint8_t)buffer[i] < 0x80)
				return val;
		}

		throw PipeException("LEB128 number larger than buffer size");
	}
	
	void WriteLEB128(uint64_t val)
	{
		char buffer[10];
		int size = EncodeLEB128(buffer, val);
		Write(buffer, size);
	}

	uint64_t ReadLEB128()
	{
		char buffer[10];
		for(int i = 0; i < (int)sizeof(buffer); i++){
			Read(buffer + i, 1);

			if((uint8_t)buffer[i] < 0x80)
				break;
		}

		return DecodeLEB128(buffer, sizeof(buffer));
	}

	void WriteInt32(int32_t val)
	{
		WriteInt((void*)&val, sizeof(int32_t));
	}

	void WriteUInt32(uint32_t val)
	{
		WriteInt((void*)&val, sizeof(uint32_t));
	}
	
	int32_t ReadInt32()
	{
		int32_t val;
		ReadInt((void*)&val, sizeof(int32_t));
		return val;
	}

	uint32_t ReadUInt32()
	{
		int32_t val;
		ReadInt((void*)&val, sizeof(uint32_t));
		return val;
	}
	
	void WriteString(const std::wstring& str)
	{
		std::string s8 = EncodeUTF8(str);
		WriteLEB128(s8.size());
		Write(s8.data(), s8.size());
	}

	void ReadString(std::wstring& str)
	{
		int size = ReadLEB128();

		std::vector<char> buffer(size);
		Read(&buffer[0], size);

		str = DecodeUTF8(buffer.data(), buffer.size());
	}
	
	void WriteBuffer(const std::vector<uint8_t>& buffer)
	{
		WriteUInt32(buffer.size());
		Write((const char*)buffer.data(), buffer.size());
	}

	void ReadBuffer(std::vector<uint8_t>& buffer)
	{
		uint32_t size = ReadUInt32();
		buffer.resize(size);
		Read((char*)&buffer[0], size);
	}
	
	void Close()
	{
		if(pipe != INVALID_HANDLE_VALUE)
			CloseHandle(pipe);

		pipe = INVALID_HANDLE_VALUE;
	}

	~CPipe()
	{
		Close();
	}
};

PipePtr Pipe::Create()
{
	return std::make_shared<CPipe>();
}
