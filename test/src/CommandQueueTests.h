#ifndef COMMANDQUEUETESTS_H
#define COMMANDQUEUETESTS_H

#include <memory>

#include "TestFixture.h"

typedef std::shared_ptr<class CommandQueueTests> CommandQueueTestsPtr;

class CommandQueueTests : public TestFixture
{
	public:
	static CommandQueueTestsPtr Create();
};

#endif

