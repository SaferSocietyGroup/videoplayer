#ifndef PIPETESTS_H
#define PIPETESTS_H

#include <memory>

#include "TestFixture.h"

typedef std::shared_ptr<class PipeTests> PipeTestsPtr;

class PipeTests : public TestFixture
{
	public:
	static PipeTestsPtr Create();
};

#endif
