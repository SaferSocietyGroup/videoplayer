#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include <memory>
#include <string>
#include <vector>

typedef std::shared_ptr<class CommandQueue> CommandQueuePtr;

class CommandQueueException : public std::runtime_error {
	public:
	CommandQueueException(std::string str) : std::runtime_error(str) {}
};

enum CommandType
{
	CTQuit =   0,
	CTPlay =   1,
	CTPause =  2, 
	CTStop =   3,
	CTSeek =   4,
	CTLoad =   5,
	CTUnload = 6,

	CTCmdCount
};

enum ArgumentType
{
	ATStr, ATInt32, ATFloat, ATDouble
};

// number of arguments for the different commands
const std::vector<std::vector<ArgumentType>> CommandArgs  = {{}, {}, {}, {}, {ATFloat}, {ATStr}, {}};

struct Argument
{
	ArgumentType type;

	int32_t i;
	std::wstring str;
	float f;
	double d;
};

struct Command
{
	CommandType type;
	std::vector<Argument> args;
};

class CommandQueue
{
	public:
	virtual void Start(const std::wstring& pipeName) = 0;
	virtual bool Dequeue(Command& cmd) = 0;

	static CommandQueuePtr Create();
};

#endif
