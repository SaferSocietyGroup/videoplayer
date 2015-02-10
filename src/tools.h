/*
 * NetClean VideoPlayer
 *  Multi process video player for windows.
 *
 * Copyright (c) 2010-2013 NetClean Technologies AB
 * All Rights Reserved.
 *
 * This file is part of NetClean VideoPlayer.
 *
 * NetClean VideoPlayer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * NetClean VideoPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NetClean VideoPlayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TOOLS_H
#define TOOLS_H

#ifndef M_PI
#define M_PI 3.14159265
#endif

#ifndef M_E
#define M_E 2.71828183
#endif

// Vectors

#define V_ITER(__type) std::vector<__type>::iterator
#define V_BITER(__type) std::vector<__type>::reverse_iterator
#define V_ITERATE(__vector, __type) for(V_ITER(__type) it = __vector.begin(); it != __vector.end(); it++)
#define V_BITERATE(__vector, __type) for(V_BITER(__type) it = __vector.rbegin(); it != __vector.rend(); ++it)

typedef unsigned char byte;

// Strings
#define Str(__what) [&]() -> std::string {std::stringstream __tmp; __tmp << __what; return __tmp.str(); }()
#define ExpStr(__exp) Str(#__exp << " = " << __exp << " ")

#define POW2(__x) ((__x) * (__x))

#include <cmath>
#include <algorithm>
#include <sstream>
#include <memory>
#include <cstdlib>

#define CLAMP(__MIN, __MAX, __VAL) std::min(std::max((__MIN), (__VAL)),( __MAX))

class Tools
{
	public:
	static inline std::string WstrToStr(const std::wstring& w)
	{
		std::unique_ptr<char> tmp = std::unique_ptr<char>(new char [w.size() * 4 + 1]());
		size_t size = wcstombs(tmp.get(), w.c_str(), w.size() * 4 + 1);
		return std::string(tmp.get(), size);
	}

	static inline std::wstring StrToWstr(const std::string& w)
	{
		std::unique_ptr<wchar_t> tmp = std::unique_ptr<wchar_t>(new wchar_t [w.size() + 2]());
		size_t size = mbstowcs(tmp.get(), w.c_str(), w.size() + 1);
		return std::wstring(tmp.get(), size);
	}
};

#endif
