#ifndef STRINGTOOLS_H
#define STRINGTOOLS_H

#include <unordered_map>
#include <string>
#include <vector>

#include "Tools.h"

typedef std::unordered_map<std::string, std::string> StrStrMap;
typedef std::vector<std::string> StrVec;

class StringTools {
	public:
	static std::string RTrim(std::string s, const std::string& trimChars = " \t");
	static std::string LTrim(std::string s, const std::string& trimChars = " \t");
	static std::string Trim(const std::string& s, const std::string& trimChars = " \t");
	static StrVec Split(const std::string& s, char delim = ' ', int count = -1);
};

#endif
