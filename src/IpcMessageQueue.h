#ifndef IPC_H
#define IPC_H

#include <memory>
#include <functional>
#include <stdexcept>

class IpcMessageQueue;
typedef std::shared_ptr<IpcMessageQueue> IpcMessageQueuePtr;

struct IpcEx : public std::runtime_error {
	IpcEx(std::string str) : std::runtime_error(str) {}
};

class IpcMessageQueue
{
	public:
	typedef std::function<void(const std::string& type, const char* buffer, size_t size)> ReadBufferFunc;
	
	//virtual bool ReadMessage(std::string& type, std::string& message, int timeout = -1) = 0;
	//virtual bool WriteMessage(std::string type, std::string message = "", int timeout = -1) = 0;
	
	//virtual bool GetReadBuffer(ReadBufferFunc func, int timeout = -1) = 0;

	//virtual char* GetWriteBuffer(int timeout = -1) = 0;
	//virtual void ReturnWriteBuffer(std::string type, char** buffer, int len) = 0;
	
	virtual int GetReadQueueSize() = 0;
	virtual int GetWriteQueueSize() = 0;

	virtual void WritePacket(const std::string& type, const std::string& packet = "") = 0;
	virtual bool ReadPacket(std::string& type, std::string& packet) = 0;
	
	static IpcMessageQueuePtr Create(std::string name, int readQueueBuffers = 4, int readQueueSize = 1024, int writeQueueBuffers = 4, int writeQueueSize = 4);
	static IpcMessageQueuePtr Open(std::string name);
};

#endif
