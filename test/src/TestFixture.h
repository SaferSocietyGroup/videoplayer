#ifndef TESTFIXTURE_H
#define TESTFIXTURE_H

#include <functional>
#include <vector>
#include <string>

#include "Tools.h"

#define TAssert(_v, _msg) if(!(_v)){ throw TestException(Str(__FILE__ ":" << __LINE__ << " " << __func__ << ": " << _msg)); }
#define TAssertEquals(_v1, _v2) TAssert((_v1) == (_v2), (_v1) << " != " << (_v2));

class TestException : public std::runtime_error {
	public:
	TestException(std::string str) : std::runtime_error(str) {}
};

class Test
{
	public:
	std::string category;
	std::string name;
	std::function<void()> fun;
};

typedef std::shared_ptr<class TestFixture> TestFixturePtr;

class TestFixture
{
	public:
	virtual void RegisterTests(std::vector<Test>& testSet) = 0;
};

#endif
