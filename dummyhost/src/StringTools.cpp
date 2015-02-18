#include "StringTools.h"

#include <algorithm>
#include <iostream>
#include <cstdlib>

StrVec StringTools::Split(const std::string& s, char delim, int count)
{
	std::vector<std::string> ret;

	std::stringstream ss(s);
	std::string item;
	size_t at = 0;

	while (std::getline(ss, item, delim)) {
		ret.push_back(item);
		at += item.size() + 1;

		if(count != -1 && count == (int)ret.size()){
			if(at < ss.str().size())
				ret.push_back(ss.str().substr(at));
			break;
		}
	}

	return ret;
}

std::string StringTools::RTrim(std::string s, const std::string& trimChars)
{
	auto l = [&](char c){ return trimChars.find(c) == std::string::npos; };
	s.erase(std::find_if(s.rbegin(), s.rend(), l).base(), s.end());
	return s;
}

std::string StringTools::LTrim(std::string s, const std::string& trimChars)
{
	auto l = [&](char c){ return trimChars.find(c) == std::string::npos; };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), l));
        return s;
}

std::string StringTools::Trim(const std::string& s, const std::string& trimChars)
{
	return LTrim(RTrim(s, trimChars), trimChars);
}
