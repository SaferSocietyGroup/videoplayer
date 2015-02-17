#include "ArgParser.h"

#include <map>
#include <queue>
#include <sstream>
#include <iomanip>

#include "Flog.h"

struct Switch
{
	char shortName;
	std::string longName;
	std::string argName;
	bool takesArg;
	std::string description;
	std::function<void()> action;
	std::function<void(const std::string& arg)> actionArg;
};

#ifndef Str
#define Str(_what) [&]() -> std::string {std::stringstream _tmp; _tmp << _what; return _tmp.str(); }()
#endif

class CArgParser : public ArgParser
{
	public:
	std::map<char, Switch> switchesShort;
	std::map<std::string, Switch> switchesLong;

	bool startsWith(const std::string& str, const std::string& needle)
	{
		if(str.size() >= needle.size())
			return str.substr(0, needle.size()) == needle;
		return false;
	}

	std::vector<std::string> Parse(int argc, char** argv)
	{
		std::queue<std::string> args;

		for(int i = 0; i < argc; i++)
			args.push(argv[i]);
		
		std::vector<std::string> ret;

		while(!args.empty()){
			std::string arg = args.front();
			args.pop();

			if(startsWith(arg, "--")){
				auto it = switchesLong.find(arg.substr(2));

				if(it == switchesLong.end())
					throw ArgParserException(Str("no such switch: " << arg));

				Switch s = it->second;

				if(s.takesArg){
					if(args.empty())
						throw ArgParserException(Str("switch expects argument: " << arg << " " << s.argName));

					s.actionArg(args.front());
					args.pop();
				}
				
				else{
					s.action();
				}
			}

			else if(startsWith(arg, "-")){
				if(arg.size() != 2)
					throw ArgParserException(Str("short name multi-switches not supported: " << arg << " (did you mean -" << arg << ")?"));

				auto it = switchesShort.find(arg[1]);

				if(it == switchesShort.end())
					throw ArgParserException(Str("no such switch: " << arg));

				Switch s = it->second;

				if(s.takesArg){
					if(args.empty())
						throw ArgParserException(Str("switch expects argument: " << arg << " " << s.argName));

					s.actionArg(args.front());
					args.pop();
				}
				
				else{
					s.action();
				}
			}
			
			else {
				ret.push_back(arg);
			}
		}

		return ret;
	}
	
	void AddSwitch(char shortName, const std::string& longName, const std::string& description,
		std::function<void()> action)
	{
		Switch s = {shortName, longName, "", false, description, action, [](const std::string& v){}};
		switchesShort[shortName] = s;
		switchesLong[longName] = s;
	}

	void AddSwitchArg(char shortName, const std::string& longName, const std::string& argName,
		const std::string& description, std::function<void(const std::string& arg)> action)
	{
		Switch s = {shortName, longName, argName, true, description, []{}, action};
		switchesShort[shortName] = s;
		switchesLong[longName] = s;
	}
	
	std::string GetHelp()
	{
		int width = 0;
		for(auto a : switchesShort){
			Switch s = a.second;
			if(s.takesArg){
				int w = Str(s.longName << " " << s.argName).size();
				width = w > width ? w : width;
			}
			else{
				int w = s.longName.size();
				width = w > width ? w : width;
			}
		}

		std::stringstream ss;
		for(auto a : switchesShort){
			Switch s = a.second;
			if(s.takesArg){
				ss << "  -" << s.shortName << ", --" << std::left << std::setw(width + 3) << Str(s.longName << " " << s.argName) << s.description << std::endl;
			}
			else
				ss << "  -" << s.shortName << ", --" << std::left << std::setw(width + 3) << s.longName << s.description << std::endl;
		}

		return ss.str();
	}
};

ArgParserPtr ArgParser::Create()
{
	return std::make_shared<CArgParser>();
}
