#ifndef PIPE_H
#define PIPE_H

#include <string>
#include <memory>
#include <cstdint>

typedef std::shared_ptr<class Pipe> PipePtr;

class PipeException : public std::runtime_error {
	public:
	PipeException(std::string str) : std::runtime_error(str) {}
};

class Pipe
{
	public:
	virtual void Open(const std::wstring& name, int msTimeout = -1) = 0;
	virtual void CreatePipe(const std::wstring& name) = 0;
	virtual void Close() = 0;

	virtual void WriteInt32(int32_t val) = 0;
	virtual void WriteUInt32(uint32_t val) = 0;
	virtual void WriteFloat(float val) = 0;
	virtual void WriteDouble(double val) = 0;

	virtual int32_t ReadInt32() = 0;
	virtual uint32_t ReadUInt32() = 0;
	virtual float ReadFloat() = 0;
	virtual double ReadDouble() = 0;

	virtual void WriteLEB128(uint64_t val) = 0;
	virtual uint64_t ReadLEB128() = 0;

	virtual int EncodeLEB128(char* buffer, uint64_t val) = 0;
	virtual uint64_t DecodeLEB128(char* buffer, int size) = 0;

	virtual void WriteString(const std::wstring& str) = 0;
	virtual void ReadString(std::wstring& str) = 0;

	static PipePtr Create();
};

#endif
