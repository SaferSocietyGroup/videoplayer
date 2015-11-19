#include <cstdarg>
#include <thread>
#include <queue>
#include <mutex>
#include <SDL.h>

#include "CommandSender.h"
#include "Pipe.h"
#include "Tools.h"

class CCommandSender : public CommandSender
{
	public:
	PipePtr pipe;

	std::string ex;
	bool wasException = false;

	std::queue<Command> queue;
	bool done = false;
	std::thread* thread = nullptr;
	std::mutex mutex;

	void Stop()
	{
		done = true;
		if(thread)
			thread->join();
	}

	void Start(PipePtr inPipe)
	{
		if(thread != nullptr)
			throw CommandSenderException("command sender double start");

		this->pipe = inPipe;

		thread = new std::thread([&](){
			try {
				while(!done){
					bool wasEmpty = false;
					Command cmd;

					{
						std::lock_guard<std::mutex> lock(mutex);
						wasEmpty = queue.empty();

						if(!wasEmpty){
							cmd = queue.front();
							queue.pop();
						}
					}

					if(!wasEmpty){
						pipe->WriteUInt32(MAGIC);
						pipe->WriteUInt32((uint32_t)cmd.type);

						pipe->WriteUInt32(cmd.seqNum);
						pipe->WriteUInt32(cmd.flags);

						int i = 0;
		
						auto argSpec = (cmd.flags & CFResponse) != 0 ? CommandSpecs[cmd.type].responseArgTypes : CommandSpecs[cmd.type].requestArgTypes;

						for(ArgumentType aType : argSpec){
							switch(aType){
								case ATStr:    pipe->WriteString(cmd.args[i].str); break;
								case ATInt32:  pipe->WriteInt32(cmd.args[i].i);    break;
								case ATFloat:  pipe->WriteFloat(cmd.args[i].f);    break;
								case ATDouble: pipe->WriteDouble(cmd.args[i].d);   break;
								case ATBuffer: pipe->WriteBuffer(cmd.args[i].buf); break;
							}

							i++;
						}

						pipe->WriteUInt32(MAGIC);
					}

					else{
						SDL_Delay(100);
					}
				}
			}

			catch (std::runtime_error e)
			{
				ex = e.what();
				wasException = true;
			}
		});
	}
	
	void WaitForConnection(int msTimeout)
	{
		pipe->WaitForConnection(msTimeout);
	}
	
	void SendCommand(uint32_t seqNum, uint32_t flags, CommandType type, ...)
	{
		Command cmd;

		cmd.type = type;
		cmd.seqNum = seqNum;
		cmd.flags = flags;

		int i = 0;

		va_list vl;
		va_start(vl, type);
					
		auto argSpec = (cmd.flags & CFResponse) != 0 ? CommandSpecs[cmd.type].responseArgTypes : CommandSpecs[cmd.type].requestArgTypes;

		for(ArgumentType aType : argSpec){
			Argument arg;
			arg.type = aType;

			switch(aType){
				case ATStr:    arg.str = va_arg(vl, const wchar_t*); break;
				case ATInt32:  arg.i = va_arg(vl, int);              break;
				case ATFloat:  arg.f = va_arg(vl, double);           break;
				case ATDouble: arg.d = va_arg(vl, double);           break;
				case ATBuffer: throw CommandSenderException("not supported"); break;
			}

			cmd.args.push_back(arg);

			i++;
		}

		va_end(vl);

		return SendCommand(cmd);
	}

	void SendCommand(Command& cmd)
	{
		if(wasException)
			throw CommandSenderException(Str("pipe threw exception: " << ex));

		auto argSpec = (cmd.flags & CFResponse) != 0 ? CommandSpecs[cmd.type].responseArgTypes : CommandSpecs[cmd.type].requestArgTypes;

		if(cmd.args.size() != argSpec.size())
			throw CommandSenderException(Str("Command expects " << argSpec.size() << " args (not " << cmd.args.size()<< ")"));

		{
			std::lock_guard<std::mutex> lock(mutex);
			queue.push(cmd);
		}
	}
};

CommandSenderPtr CommandSender::Create()
{
	return std::make_shared<CCommandSender>();
}
