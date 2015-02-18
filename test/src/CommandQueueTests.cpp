#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <limits>
#include <string>
#include <thread>

#include "CommandQueueTests.h"
#include "CommandQueue.h"
#include "Flog.h"
#include "Pipe.h"

#define MAGIC 0xaabbaacc

class CCommandQueueTests : public CommandQueueTests
{
	public:
	void RegisterTests(std::vector<Test>& testSet)
	{
		testSet.push_back({"CommandQueue", "StartStop", [&]{StartStop();} });
		testSet.push_back({"CommandQueue", "DecodeCommands", [&]{DecodeCommands();} });
	}

	void StartStop()
	{
		PipePtr pipe = Pipe::Create();
		pipe->CreatePipe(L"cmd");

		CommandQueuePtr cq = CommandQueue::Create();

		cq->Start(L"cmd");

		// stop
		pipe->WriteUInt32(MAGIC);
		pipe->WriteUInt32(CTQuit);
		pipe->WriteUInt32(MAGIC);
	}

	void DecodeCommands()
	{
		PipePtr pipe = Pipe::Create();
		pipe->CreatePipe(L"cmd");

		CommandQueuePtr cq = CommandQueue::Create();

		cq->Start(L"cmd");
		
		std::thread t([&](){
			pipe->WriteUInt32(MAGIC);
			pipe->WriteUInt32(CTPlay);
			pipe->WriteUInt32(MAGIC);
			
			pipe->WriteUInt32(MAGIC);
			pipe->WriteUInt32(CTPause);
			pipe->WriteUInt32(MAGIC);
			
			pipe->WriteUInt32(MAGIC);
			pipe->WriteUInt32(CTStop);
			pipe->WriteUInt32(MAGIC);
			
			pipe->WriteUInt32(MAGIC);
			pipe->WriteUInt32(CTSeek);
			pipe->WriteFloat(0.34f);
			pipe->WriteUInt32(MAGIC);

			pipe->WriteUInt32(MAGIC);
			pipe->WriteUInt32(CTLoad);
			pipe->WriteString(L"name");
			pipe->WriteUInt32(MAGIC);
			
			pipe->WriteUInt32(MAGIC);
			pipe->WriteUInt32(CTUnload);
			pipe->WriteUInt32(MAGIC);
			
			pipe->WriteUInt32(MAGIC);
			pipe->WriteUInt32(CTQuit);
			pipe->WriteUInt32(MAGIC);
		});

		int nCmd = 0;

		while(true){
			Command c;
			if(cq->Dequeue(c)){
				nCmd++;

				CommandType type = (CommandType)nCmd;

				TAssertEquals(c.type, type);
				TAssertEquals(CommandArgs[nCmd].size(), c.args.size());

				if(c.type == CTLoad){
					TAssert(c.args[0].str == L"name", "string mismatch");
				}
				
				if(c.type == CTSeek){
					TAssertEquals(c.args[0].f, 0.34f);
				}

				if(nCmd >= 6)
					break;
			}
		}

		t.join();
	}
};

CommandQueueTestsPtr CommandQueueTests::Create()
{
	return std::make_shared<CCommandQueueTests>();
}
