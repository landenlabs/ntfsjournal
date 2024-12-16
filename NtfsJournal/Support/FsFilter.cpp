// ------------------------------------------------------------------------------------------------
// Filter classes used to limit output of file system scan by filtering on 
// name, date or size.
//
// Author:  Dennis Lang   Apr-2011
// https://lanenlabs.com
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

