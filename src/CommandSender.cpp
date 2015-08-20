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
			throw CommandQueueException("command sender double start");

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

						int i = 0;

						for(ArgumentType aType : CommandArgs[cmd.type]){
							switch(aType){
								case ATStr:    pipe->WriteString(cmd.args[i].str); break;
								case ATInt32:  pipe->WriteInt32(cmd.args[i].i);    break;
								case ATFloat:  pipe->WriteFloat(cmd.args[i].f);    break;
								case ATDouble: pipe->WriteDouble(cmd.args[i].d);   break;
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
	
	void SendCommand(CommandType type, ...)
	{
		Command cmd;

		cmd.type = type;

		int i = 0;

		va_list vl;
		va_start(vl, type);

		for(ArgumentType aType : CommandArgs[cmd.type]){
			Argument arg;
			arg.type = aType;

			switch(aType){
				case ATStr:    arg.str = va_arg(vl, const wchar_t*); break;
				case ATInt32:  arg.i = va_arg(vl, int);              break;
				case ATFloat:  arg.f = va_arg(vl, double);           break;
				case ATDouble: arg.d = va_arg(vl, double);           break;
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

		if(cmd.args.size() != CommandArgs[cmd.type].size())
			throw CommandSenderException(Str("Command expects " << CommandArgs[cmd.type].size() << " args (not " << cmd.args.size()<< ")"));

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
