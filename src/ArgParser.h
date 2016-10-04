#ifndef ARGPARSER_H
#define ARGPARSER_H

#include <memory>
#include <functional>
#include <string>
#include <stdexcept>
#include <vector>

class ArgParserException : public std::runtime_error {
	public:
	ArgParserException(std::string str) : std::runtime_error(str) {}
};

typedef std::shared_ptr<class ArgParser> ArgParserPtr;

class ArgParser
{
	public:
	virtual void AddSwitch(char shortName, const std::string& longName, const std::string& description,
		std::function<void()> action) = 0;

	virtual void AddSwitchArg(char shortName, const std::string& longName, const std::string& argName,
		const std::string& description, std::function<void(const std::string& arg)> action) = 0;
	
	virtual std::vector<std::string> Parse(int argc, char** argv) = 0;

	virtual std::string GetHelp() = 0;

	static ArgParserPtr Create();
};

#endif
