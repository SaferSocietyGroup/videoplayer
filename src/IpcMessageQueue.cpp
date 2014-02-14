#include "IpcMessageQueue.h"
#include "Flog.h"
#include "Tools.h"

#include <libshmipc.h>
//#include <mutex>

class CIpcMessageQueue : public IpcMessageQueue {
	shmipc* readQueue, *writeQueue;
	//std::mutex readLock, writeLock;

	public:
	CIpcMessageQueue(std::string name, bool isHost = false, int readQueueBuffers = 0, int readQueueSize = 0, int writeQueueBuffers = 0, int writeQueueSize = 0){
		std::string writeName = name;
		std::string readName = name;

		std::string* n = isHost ? &writeName : &readName;

		n->append("_host_writer");

		if(isHost){	
			auto err = shmipc_create(writeName.c_str(), writeQueueSize, writeQueueBuffers, SHMIPC_AM_WRITE, &writeQueue);
			AssertEx(err == SHMIPC_ERR_SUCCESS, IpcEx, "could not create writer");
			
			err = shmipc_create(readName.c_str(), readQueueSize, readQueueBuffers, SHMIPC_AM_READ, &readQueue);
			AssertEx(err == SHMIPC_ERR_SUCCESS, IpcEx, "could not create reader");
		}else{
			auto err = shmipc_open(writeName.c_str(), SHMIPC_AM_WRITE, &writeQueue);
			AssertEx(err == SHMIPC_ERR_SUCCESS, IpcEx, "could not open writer");

			err = shmipc_open(readName.c_str(), SHMIPC_AM_READ, &readQueue);
			AssertEx(err == SHMIPC_ERR_SUCCESS, IpcEx, "could not open writer");
		}
	}

	bool ReadMessage(std::string& type, std::string& message, int timeout) {
		char cType[SHMIPC_MESSAGE_TYPE_LENGTH];
		size_t length;
		const char* buffer;

		auto err =  shmipc_acquire_buffer_r(readQueue, cType, &buffer, &length, timeout);

		AssertEx(err == SHMIPC_ERR_SUCCESS || err == SHMIPC_ERR_TIMEOUT, IpcEx, "failed to acquire read buffer");

		if(err == SHMIPC_ERR_SUCCESS){
			message.assign(buffer, length);
			type.assign(cType);
			shmipc_return_buffer_r(readQueue, &buffer);
			return true;
		}

		return false;
	}

	bool WriteMessage(std::string type, std::string message, int timeout) {
		AssertEx(type.length() < SHMIPC_MESSAGE_TYPE_LENGTH, IpcEx, "type too long (" << type.length() << " / " << SHMIPC_MESSAGE_TYPE_LENGTH << ")"); 
		auto err = shmipc_send_message(writeQueue, type.c_str(), message.c_str(), message.length(), timeout);
		AssertEx(err == SHMIPC_ERR_SUCCESS || err == SHMIPC_ERR_TIMEOUT, IpcEx, "failed to send message");
		return (err == SHMIPC_ERR_SUCCESS);
	}

	bool GetReadBuffer(ReadBufferFunc func, int timeout = -1){
		char type[SHMIPC_MESSAGE_TYPE_LENGTH]; 

		const char* buffer;
		size_t size;

		auto err = shmipc_acquire_buffer_r(readQueue, type, &buffer, &size, timeout);

		AssertEx(err == SHMIPC_ERR_SUCCESS || err == SHMIPC_ERR_TIMEOUT, IpcEx, "failed to acquire read buffer");

		if(err == SHMIPC_ERR_SUCCESS){
			func(type, buffer, size);

			auto err = shmipc_return_buffer_r(readQueue, &buffer);
			AssertEx(err == SHMIPC_ERR_SUCCESS, IpcEx, "failed to return read buffer");

			return true;
		}

		return false;
	}

	char* GetWriteBuffer(int timeout){
		char* ret = 0;
		auto err = shmipc_acquire_buffer_w(writeQueue, &ret, timeout);

		AssertEx(err == SHMIPC_ERR_SUCCESS || err == SHMIPC_ERR_TIMEOUT, IpcEx, "failed to acquire write buffer");
		
		return ret;
	}

	void ReturnWriteBuffer(std::string type, char** buffer, int len){
		AssertEx(type.length() < SHMIPC_MESSAGE_TYPE_LENGTH, IpcEx, "type too long (" << type.length() << " / " << SHMIPC_MESSAGE_TYPE_LENGTH << ")"); 
		auto err = shmipc_return_buffer_w(writeQueue, buffer, len, type.c_str());
		AssertEx(err == SHMIPC_ERR_SUCCESS, IpcEx, "failed to return read buffer");
	}
	
	int GetReadQueueSize(){
		return shmipc_get_buffer_size(readQueue);
	}

	int GetWriteQueueSize(){
		return shmipc_get_buffer_size(writeQueue);
	}

	~CIpcMessageQueue(){
		FlogD("destroying queue");
		shmipc_destroy(&writeQueue);
		shmipc_destroy(&readQueue);
	}
};

IpcMessageQueuePtr IpcMessageQueue::Create(std::string name, int readQueueBuffers, int readQueueSize, int writeQueueBuffers, int writeQueueSize)
{
	return IpcMessageQueuePtr(new CIpcMessageQueue(name, true, readQueueBuffers, readQueueSize, writeQueueBuffers, writeQueueSize));
}

IpcMessageQueuePtr IpcMessageQueue::Open(std::string name)
{
	return IpcMessageQueuePtr(new CIpcMessageQueue(name));
}
