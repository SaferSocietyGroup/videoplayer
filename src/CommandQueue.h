#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include <memory>
#include <string>
#include <vector>

#include "Pipe.h"
#include "Protocol.h"

typedef std::shared_ptr<class CommandQueue> CommandQueuePtr;

class CommandQueueException : public std::runtime_error {
	public:
	CommandQueueException(std::string str) : std::runtime_error(str) {}
};

class CommandQueue
{
	public:
	virtual void Start(PipePtr pipe) = 0;
	virtual bool Dequeue(Command& cmd) = 0;
	virtual void WaitForConnection(int msTimeout) = 0;

	static CommandQueuePtr Create();
};

#endif
