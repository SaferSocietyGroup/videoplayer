#include <SDL.h>
#include <iostream>
#include <string>
#include <map>
#include <iomanip>
#include <stdexcept>

#include "ArgParser.h"
#include "Flog.h"

#include "PipeTests.h"
#include "CommandQueueTests.h"

int main(int argc, char** argv)
{
	std::vector<Test> tests;
	PipeTests::Create()->RegisterTests(tests);
	CommandQueueTests::Create()->RegisterTests(tests);

	try {
		bool showHelp = false;

		ArgParserPtr arg = ArgParser::Create();

		std::string category = "*", test = "*";
		bool list = false;

		arg->AddSwitch('h', "help", "Show help.", [&](){ showHelp = true; });
		arg->AddSwitch('l', "list", "List available tests.", [&](){ list = true; });
		
		arg->AddSwitchArg('c', "category", "CATEGORY", "Specify category to run tests from, or '*' for all.", 
			[&](const std::string& arg){ category = arg; });
		
		arg->AddSwitchArg('t', "test", "TEST", "Specify test to run, or '*' for all.", 
			[&](const std::string& arg){ test = arg; });

		std::vector<std::string> rest = arg->Parse(argc, argv);

		if(showHelp)
		{
			std::cout << "Usage: " << argv[0] << " [OPTION]..." << std::endl;
			std::cout << arg->GetHelp();
			return 0;
		}
		
		int catMaxLen = 8, testMaxLen = 4;

		for(auto t : tests){
			if(catMaxLen < (int)t.category.size())
				catMaxLen = t.category.size();
			
			if(testMaxLen < (int)t.name.size())
				testMaxLen = t.name.size();
		}

		std::cout << std::left << std::setw(catMaxLen) << "Category" << " " << 
			std::left << std::setw(testMaxLen) << "Test" << " result" << std::endl;

		std::cout << std::setfill('=') << std::setw(catMaxLen + testMaxLen + 8) << "=" << std::endl;
		std::cout << std::setfill(' ');

		for(auto t : tests)
		{
			if((category == "*" || category == t.category) && (test == "*" || test == t.name)){
				std::cout << std::left << std::setw(catMaxLen) << t.category << " " << std::left << std::setw(testMaxLen) << t.name;
				
				if(!list){
					try {
						t.fun();
						std::cout << " ok" << std::endl;
					}
					
					catch (std::runtime_error e)
					{
						std::cout << " failed - " << e.what() << std::endl;
					}

					catch (std::exception e)
					{
						std::cout << " failed - " << e.what() << std::endl;
					}
				}

				else{
					std::cout << std::endl;
				}
			}
		}
	}

	catch (std::exception ex)
	{
		FlogF(ex.what());
		return 1;
	}

	return 0;
}
