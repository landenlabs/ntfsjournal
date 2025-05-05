// ------------------------------------------------------------------------------------------------
// FileSystem filter classes used to limit output of file system scan by filtering on 
// name, date or size.
//
// Author:  Dennis Lang   Apr-2011
// https://landenlabs.com
//
// ----- License ----
//
// Copyright (c) 2014 Dennis Lang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// ------------------------------------------------------------------------------------------------

#include "FsFilter.h"
#include "Pattern.h"

#include <Windows.h>
#include <time.h>

#include <iostream>
#include <regex>

// ------------------------------------------------------------------------------------------------

bool IsDateModifyGreater(const LARGE_INTEGER& fileTime, const FILETIME& knownTime)
{
    return (CompareFileTime((const FILETIME*)&fileTime, &knownTime) > 0);
}

bool IsDateModifyEqual(const LARGE_INTEGER& fileTime, const FILETIME& knownTime)
{
    return (CompareFileTime((const FILETIME*)&fileTime, &knownTime) == 0);
}

bool IsDateModifyLess(const LARGE_INTEGER& fileTime, const FILETIME& knownTime)
{
    return (CompareFileTime((const FILETIME*)&fileTime, &knownTime) < 0);
}


// ------------------------------------------------------------------------------------------------

bool IsNameIcase(const std::wstring& name, const std::wstring& pattern)
{
    // return (wcsnicmp((wchar_t*)aName.wFilename, name.c_str(), aName.chFileNameLength) == 0);
    return Pattern::CompareNoCase(pattern.c_str(), name.c_str());
}

bool IsName(const std::wstring& name, const std::wstring& pattern)
{
    // return (wcsncmp((wchar_t*)aName.wFilename, name.c_str(), aName.chFileNameLength) == 0);
    return Pattern::CompareCase(pattern.c_str(), name.c_str());
}


// ------------------------------------------------------------------------------------------------

bool IsGrepIcase(const std::wstring& name, const std::wregex& regPattern)
{
    return std::regex_match(name, regPattern);
}

// ------------------------------------------------------------------------------------------------

bool IsSizeGreater(LONGLONG size, LONGLONG knownSize)
{
    return (size > knownSize);
}

bool IsSizeEqual(LONGLONG size, LONGLONG knownSize)
{
    return (size == knownSize);
}

bool IsSizeLess(LONGLONG size, LONGLONG knownSize)
{
    return (size < knownSize);
}

