// ------------------------------------------------------------------------------------------------
// Filter classes used to limit output of file system scan.
//
// Author:  Dennis Lang   Apr-2011
// https://landenlabs.com
// ------------------------------------------------------------------------------------------------


#pragma once

#include "BaseTypes.h"
#include "FsTime.h"

#include <string>
#include <time.h>
#include <regex>

// ------------------------------------------------------------------------------------------------
// Abstract base matching class.
template <typename dataType>
class Match
{
public:
    Match(bool matchOn = true) :
       m_matchOn(matchOn)
    { }

    virtual bool IsMatch(const dataType& data, const void* pData) = 0;

    bool m_matchOn;
};

// ------------------------------------------------------------------------------------------------
template <typename dataType>
class FsFilter
{
public:
    virtual bool IsMatch(const dataType&, const void* pData) const = 0;
    virtual bool IsValid() const = 0;
};

// ------------------------------------------------------------------------------------------------
//  Single filter rule
//  Ex:
//      OneFilter oneFilter(new MatchName("*.txt", IsNameIcase));
//      ...user filter
//      FILETIME today = ...
//      oneFilter.SetMatch(new MatchDate(daysAgo, IsDateModifyGreater));
//
template <typename dataType>
class OneFilter : public FsFilter<dataType>
{
public:
    OneFilter() 
    { }

    OneFilter(SharePtr<Match<dataType>>& rMatch) : m_rMatch(rMatch) 
    { }

    void SetMatch(SharePtr<Match<dataType>>& rMatch)
    { m_rMatch = rMatch; }

    virtual bool IsMatch(const dataType& data, const void* pData) const
    {
        return m_rMatch->IsMatch(data, pData);
    }

    virtual bool IsValid() const
    { return !m_rMatch.IsNull();  }

private:
    SharePtr<Match<dataType>> m_rMatch;
};

// ------------------------------------------------------------------------------------------------
//  Multiple filter rules
//  Ex:
//      // All rules need to be true for match to occur  (rules are ANDed)
//      MultiFilter mFilter;
//      mFilter.List().push_back(new MatchName(L"foo"));
//      mFilter.List().push_back(new MatchName(L"*.txt", IsNameIcase, false));  // reverse match
//      mFilter.List().push_back(new MatchDate(Today, IsDateModifyGreater);
//
template <typename dataType>
class MultiFilter : public FsFilter<dataType>
{
public:
    typedef std::vector<SharePtr<Match<dataType>>> MatchList;

    MultiFilter() 
    { }

    MultiFilter(const MatchList& matchList) : m_testList(matchList) 
    { }

    void SetMatch(const MatchList& matchList)
    { m_testList = matchList; }

    MatchList& List()
    { return m_testList; }
   
    virtual bool IsMatch(const dataType& data, const void* pData) const
    {
        for (unsigned mIdx = 0; mIdx < m_testList.size(); mIdx++)
        {
            if (!m_testList[mIdx]->IsMatch(data, pData))
                return false;
        }
        return true;
    }

    virtual bool IsValid() const
    { return m_testList.size() != 0; }

private:
    MatchList  m_testList;
};


// ------------------------------------------------------------------------------------------------
//  Custom match functions for Journal Records.
// ------------------------------------------------------------------------------------------------

#include "ntfs.h"
typedef Ntfs::JournalRecord JRecord;

//
// Date matching Test filters:
//
extern bool IsDateModifyGreater(const LARGE_INTEGER&, const FILETIME& );
extern bool IsDateModifyEqual(const LARGE_INTEGER&, const FILETIME& );
extern bool IsDateModifyLess(const LARGE_INTEGER&, const FILETIME& );

// ------------------------------------------------------------------------------------------------
class MatchDate : public Match<JRecord>
{
public:
    typedef bool (*Test)(const LARGE_INTEGER&, const FILETIME& );
    MatchDate(const FILETIME& fileTime, Test test = IsDateModifyGreater, bool matchOn = true) :
        Match<JRecord>(matchOn),
        m_fileTime(fileTime), m_test(test)
    { }

    virtual bool IsMatch(const JRecord& jRecord, const void* pData)
    {
        return m_test(jRecord.m_timestamp, m_fileTime) == m_matchOn;
    }

    FILETIME m_fileTime;
    Test     m_test;
};

//
// Name matching Test filters:
//
extern bool IsNameIcase(const std::wstring&, const std::wstring& namePattern);   // Ignore case
extern bool IsName(const std::wstring&, const std::wstring& namePattern);       // currently not working.

extern bool IsGrepIcase(const std::wstring&, const std::wregex& namePattern);   // Ignore case
               
// ------------------------------------------------------------------------------------------------
class MatchName : public Match<JRecord>
{
public:
    typedef bool (*PatTest)(const std::wstring& filename, const std::wstring& namePattern);
    typedef bool (*RegTest)(const std::wstring& filename, const std::wregex& namePattern);

    MatchName(const std::wstring& name, PatTest patTest = IsNameIcase, bool matchOn = true) :
        Match(matchOn),
        m_type(ePattern), m_name(name), m_patTest(patTest)
    { }

    MatchName(const std::wregex& regPat, RegTest regTest = IsGrepIcase, bool matchOn = true) :
        Match(matchOn),
        m_type(eRegExp), m_regPat(regPat), m_regTest(regTest)
    { }

    virtual bool IsMatch(const JRecord& jRecord, const void* pData)
    {
        switch (m_type)
        {
        default:
        case ePattern:
            return (m_patTest(jRecord.m_filename, m_name)) == m_matchOn;
        case eRegExp:
            return (m_regTest(jRecord.m_filename, m_regPat)) == m_matchOn;
        }
    }

    enum MatchType { ePattern, eRegExp };
    MatchType    m_type;

    std::wstring m_name;
    PatTest      m_patTest;

    std::wregex  m_regPat;
    RegTest      m_regTest;
};



//
// Size matching Test filters:
//
extern bool IsSizeGreater(LONGLONG, LONGLONG size);
extern bool IsSizeEqual(LONGLONG, LONGLONG size);
extern bool IsSizeLess(LONGLONG, LONGLONG size);

// ------------------------------------------------------------------------------------------------
class MatchSize : public Match<JRecord>
{
public:
    typedef bool (*Test)(LONGLONG, LONGLONG size);

    MatchSize(LONGLONG size, Test test = IsSizeGreater, bool matchOn = true) :
        Match(matchOn),
        m_size(size), m_test(test) 
    { }

    virtual bool IsMatch(const JRecord& jRecord, const void* pData)
    {
        return m_test(jRecord.m_length.QuadPart, m_size) == m_matchOn;
    }

    LONGLONG     m_size;
    Test         m_test;
};

