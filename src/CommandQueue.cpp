#include <thread>
#include <mutex>
#include <queue>
#include <iomanip>

#include "CommandQueue.h"
#include "Pipe.h"
#include "Tools.h"
#include "Flog.h"

#define MAGIC 0xaabbaacc

#include "windows.h"

class CCommandQueue : public CommandQueue
{
	public:
	enum Status
	{
		SOk, SUnknownError
	};

	PipePtr pipe;
	std::string pipeEx;
	bool wasException = false;

	std::queue<Command> queue;
	bool done = false;
	std::thread* thread = nullptr;
	std::mutex mutex;

	void Start(const std::wstring& pipeName)
	{
		if(thread != nullptr)
			throw CommandQueueException("command queue double start");

		pipe = Pipe::Create();

		pipe->Open(pipeName);

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

					for(auto type : CommandArgs[cmd.type]){
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

					//std::lock_guard<std::mutex> lock(mutex);
					mutex.lock();
					queue.push(cmd);
					mutex.unlock();
					
					if(cmd.type == CTQuit)
						done = true;
				}
			}

			catch (PipeException e)
			{
				pipeEx = e.what();
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
			throw CommandQueueException(Str("pipe threw exception: " << pipeEx));
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
