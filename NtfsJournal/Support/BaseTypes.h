// ------------------------------------------------------------------------------------------------
// Base types used by project.
//
// Project: NtfsJournal
// Author:  Dennis Lang   Apr-2011
// https://lanenlabs.com
// ------------------------------------------------------------------------------------------------

#pragma once

#include <Windows.h>

#include "Hnd.h"
#include "SharePtr.h"
#include <vector>

typedef DWORD FileAttr;
typedef DWORD FileInfo;

#if 0
class Buffer : public std::vector<BYTE>
{
public:
    // data() is part of new STL available in VS2010.
    // Emulate with Data() method.
    // Return inernal pointer to beginning of active region or reserved memory.
    BYTE* Data() 
    {  return (this->_Myfirst);  }

    // Return subregion of buffer. 
    // Note - this is expensive, it creates a copy of the region.
    Buffer Region(size_t off, size_t len)
    {
        if (off + len > size())
            throw off;

        Buffer region;
        region.resize(len);
        memcpy(&region[0], Data() + off, len);
        return region;
    }
};
#endif

