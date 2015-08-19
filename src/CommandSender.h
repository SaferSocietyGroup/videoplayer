#ifndef COMMANDSENDER_H
#define COMMANDSENDER_H

#include <memory>
#include <string>
#include <vector>

typedef std::shared_ptr<class CommandSender> CommandSenderPtr;

class CommandSenderException : public std::runtime_error {
	public:
	CommandSenderException(std::string str) : std::runtime_error(str) {}
};

#include "CommandQueue.h"

class CommandSender
{
	public:
	virtual void Start(PipePtr pipe) = 0;
	virtual void SendCommand(Command& cmd) = 0;
	virtual void SendCommand(CommandType type, ...) = 0;
	virtual void WaitForConnection(int msTimeout) = 0;
	virtual void Stop() = 0;

	static CommandSenderPtr Create();
};

#endif
