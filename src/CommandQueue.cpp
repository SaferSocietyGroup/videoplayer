#include <thread>
#include <mutex>
#include <queue>
#include <iomanip>

#include "CommandQueue.h"
#include "Pipe.h"
#include "Tools.h"
#include "Flog.h"

#include "windows.h"

class CCommandQueue : public CommandQueue
{
	public:
	enum Status
	{
		SOk, SUnknownError
	};

	PipePtr pipe;
	std::string ex;
	bool wasException = false;

	std::queue<Command> queue;
	bool done = false;
	std::thread* thread = nullptr;
	std::mutex mutex;
	
	void WaitForConnection(int msTimeout)
	{
		pipe->WaitForConnection(msTimeout);
	}

	void Start(PipePtr inPipe)
	{
		if(thread != nullptr)
			throw CommandQueueException("command queue double start");

		this->pipe = inPipe;

		thread = new std::thread([&](){
			try {
				while(!done){
					uint32_t magic = pipe->ReadUInt32();

					if(magic != MAGIC)
						throw CommandQueueException(Str("corrupt message (incorrect magic at start of message), magic: " << std::hex << magic));
					
					Command cmd;
					
					cmd.type = (CommandType)pipe->ReadUInt32();

					if((uint32_t)cmd.type >= CTCmdCount)
						throw CommandQueueException(Str("unknown command type: " << (uint32_t)cmd.type));

					cmd.seqNum = pipe->ReadUInt32();
					cmd.flags = pipe->ReadUInt32();

					auto argSpec = (cmd.flags & CFResponse) != 0 ? CommandSpecs[cmd.type].responseArgTypes : CommandSpecs[cmd.type].requestArgTypes;

					for(auto type : argSpec){
						Argument arg;
						
						switch(type){
							case ATStr:    pipe->ReadString(arg.str);  break;
							case ATInt32:  arg.i = pipe->ReadInt32();  break;
							case ATFloat:  arg.f = pipe->ReadFloat();  break;
							case ATDouble: arg.d = pipe->ReadDouble(); break;
						}
						
						cmd.args.push_back(arg);
					}
					
					magic = pipe->ReadUInt32();

					if(magic != MAGIC)
						throw CommandQueueException(Str("corrupt message (incorrect magic at end of message), cmd type: " << cmd.type << ", magic: " << std::hex << magic));

					mutex.lock();
					queue.push(cmd);
					mutex.unlock();
					
					if(cmd.type == CTQuit){
						done = true;
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

	void Stop()
	{
		if(thread == nullptr)
			return;

		done = true;

		pipe->Close();

		thread->join();

		delete thread;
		thread = nullptr;
	}

	bool Dequeue(Command& cmd)
	{
		std::lock_guard<std::mutex> lock(mutex);

		if(queue.size() > 0){
			cmd = queue.front();
			queue.pop();
			return true;
		}

		else if(wasException)
		{
			throw CommandQueueException(Str("pipe threw exception: " << ex));
		}

		return false;
	}

	~CCommandQueue()
	{
		if(thread != nullptr)
			Stop();
	}
};

CommandQueuePtr CommandQueue::Create()
{
	return std::make_shared<CCommandQueue>();
}
