// ------------------------------------------------------------------------------------------------
// Windows NTFS  Journal access class.
// USN = update sequence number.
//
// Author:  Dennis Lang   July-2011
// https://landenlabs.com
// ------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <map>

#include "Hnd.h"
#include "ntfstypes.h"

using namespace std;

class Ntfs
{
public:
    Ntfs(void);
    ~Ntfs(void);
    
    bool OpenDrive(wchar_t driveLetter);
    void CloseDrive();
    wchar_t GetDrive() const
    { return m_drive; }

    bool IsOpen() const
    { return m_volHnd.IsValid(); }

    void SaveLastError(DWORD error=0) const;
    wstring GetLastErrorMsg() const
    { return m_errorMsg; }

     /*
     USN_REASON_DATA_OVERWRITE       
     USN_REASON_DATA_EXTEND          
     USN_REASON_DATA_TRUNCATION      
     USN_REASON_NAMED_DATA_OVERWRITE 
     USN_REASON_NAMED_DATA_EXTEND    
     USN_REASON_NAMED_DATA_TRUNCATION
     USN_REASON_FILE_CREATE          
     USN_REASON_FILE_DELETE          
     USN_REASON_EA_CHANGE            
     USN_REASON_SECURITY_CHANGE      
     USN_REASON_RENAME_OLD_NAME      
     USN_REASON_RENAME_NEW_NAME      
     USN_REASON_INDEXABLE_CHANGE     
     USN_REASON_BASIC_INFO_CHANGE    
     USN_REASON_HARD_LINK_CHANGE     
     USN_REASON_COMPRESSION_CHANGE   
     USN_REASON_ENCRYPTION_CHANGE    
     USN_REASON_OBJECT_ID_CHANGE     
     USN_REASON_REPARSE_POINT_CHANGE 
     USN_REASON_STREAM_CHANGE        
     USN_REASON_TRANSACTED_CHANGE    
     USN_REASON_CLOSE                
     */
    typedef DWORD UsnFilter;
    static const UsnFilter sDefaultFilter = 0
        | USN_REASON_DATA_OVERWRITE
        | USN_REASON_DATA_EXTEND
        | USN_REASON_DATA_TRUNCATION
        | USN_REASON_EA_CHANGE
        | USN_REASON_ENCRYPTION_CHANGE
        | USN_REASON_FILE_CREATE
        | USN_REASON_FILE_DELETE
        | USN_REASON_HARD_LINK_CHANGE
        | USN_REASON_RENAME_OLD_NAME | USN_REASON_RENAME_NEW_NAME 
        | USN_REASON_SECURITY_CHANGE
        ;
    void SetFilter(UsnFilter = sDefaultFilter);

    // Return true if journal available.
    bool HasJournal() const;

    struct JournalRecord
    {
	    USN				m_usn;
	    DWORD			m_reason;
	    DWORDLONG		m_fileId;
	    LARGE_INTEGER	m_timestamp;
        LARGE_INTEGER   m_length;
        DWORD           m_fileAttr;
	    wstring			m_filename;
    };

    /// Get NTFS USN Journal records which match filter.
    typedef vector<JournalRecord> JournalList;
    bool GetJournal(JournalList& list, USN startUsn=0, DWORD filter=0, bool getFileLength=false, bool getFullPath=true);

    typedef void (*HandleRecordCb)(JournalRecord& jRec, void* cbData);
    bool GetJournal(HandleRecordCb, void* cbData, USN startUsn=0, DWORD filter=0, bool getFileLength = false, bool getFullPath = true);

    static const wchar_t* GetReasonString(DWORD dwReason,  std::wstring& outReasonStr);
    static const wchar_t* GetTimestamp(const LARGE_INTEGER& timestamp, std::wstring& outTimeStr,
           const wchar_t* dateFmt = L"dd-MMM-yyyy", const wchar_t* timeFmt = L"HH:mm");


    enum GetInfo { eGetPath = 0, eGetLength=1, eCacheIt=2 };
    bool GetFileInfo(DWORDLONG objFRN, GetInfo, std::wstring& fullPath, LARGE_INTEGER& allocatedSize);

    // Get Directory information, return false if unable to get info.
    bool GetDirInfo(DWORDLONG dirFRN, std::wstring& fullPath)
    {
        LARGE_INTEGER dummy;
        return GetFileInfo(dirFRN, eCacheIt, fullPath, dummy);
    }

    // Get File information, return false if unable to get info.
    bool GetFileInfo(DWORDLONG fileFRN, std::wstring& fullPath, LARGE_INTEGER& allocatedSize)
    {
        return GetFileInfo(fileFRN, eGetLength, fullPath, allocatedSize);
    }

    USN GetNextUsn() const
    { return m_nextUsn; }

private:
    bool QueryJournal(USN_JOURNAL_DATA& usnJournalData) const;
    bool ReadJournal(
            USN& usn, 
            DWORDLONG UsnJournalID, 
            HandleRecordCb handleCb,
            void* cbData,
            JournalList* pList, 
            bool getFileSize,
            bool getFullPath,
            unsigned maxRecords = 100);

private:
	wchar_t					m_drive;
    Hnd					    m_volHnd;
	mutable wstring		    m_errorMsg;

    // NTFS USN Journal
	std::vector<byte>	    m_buffer;           
	DWORD                   m_filter;
    USN                     m_nextUsn;

    // Improve performance, remember parent path.
    struct InfoCache
    {
        InfoCache() { }
        InfoCache(const std::wstring& path, LARGE_INTEGER size) :
            filePath(path), allocatedSize(size) {}
        std::wstring  filePath;
        LARGE_INTEGER allocatedSize;
    };
    typedef std::map<DWORDLONG, InfoCache> FileInfoCache;
    FileInfoCache  m_fileInfoCache;
};


#define OBJ_CASE_INSENSITIVE      0x00000040L
#define FILE_NON_DIRECTORY_FILE   0x00000040
#define FILE_OPEN_BY_FILE_ID      0x00002000
#define FILE_OPEN                 0x00000001

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#endif

#ifndef NT_ERROR
#define NT_ERROR(Status) ((ULONG)(Status) >> 30 == 3)
#endif
