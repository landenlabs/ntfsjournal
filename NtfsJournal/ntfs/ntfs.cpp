// ------------------------------------------------------------------------------------------------
// Windows NTFS  Journal access class.
//
// Author:  Dennis Lang   July-2011
// https://lanenlabs.com
// ------------------------------------------------------------------------------------------------

#include <Windows.h>
#include <Winternl.h>
#include <iostream>

#include "Ntfs.h"
#include "fsutil.h"
#include "winerrhandlers.h"

const wchar_t sSlashStr[] = L"\\";

// ------------------------------------------------------------------------------------------------
Ntfs::Ntfs(void) : 
    m_drive('c'),
    m_nextUsn(0),
    m_filter(sDefaultFilter)
{
}

// ------------------------------------------------------------------------------------------------
Ntfs::~Ntfs(void)
{
}

// ------------------------------------------------------------------------------------------------
bool Ntfs::OpenDrive(wchar_t driveLetter)
{
    /* 
    DWORD error;
    unsigned phyDrvNum = 0;
    unsigned partitionNum = 0;

    wchar_t volumePath[] = L"\\\\.\\?:";
    volumePath[4] = towupper(driveLetter);

    error = FsUtil::GetDriveAndPartitionNumber(volumePath, phyDrvNum, partitionNum);
    if (error != ERROR_SUCCESS) {
        std::wcerr << "Error " << WinErrHandlers::ErrorMsg(error).c_str() << std::endl;
        return error;
    }

    wchar_t physicalDrive[] = L"\\\\.\\PhysicalDrive0";
    // ARRAYSIZE includes string terminating null, so backup 2 characters.
    physicalDrive[ARRAYSIZE(physicalDrive) - 2] += (char)phyDrvNum;

    */

    m_drive = driveLetter;
    m_fileInfoCache.clear();
 

    TCHAR szVolumePath[MAX_PATH];
    wsprintf(szVolumePath, TEXT("\\\\.\\%C:"), m_drive);
    m_volHnd = CreateFile(szVolumePath, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (m_volHnd == INVALID_HANDLE_VALUE) 
         SaveLastError();				

    return (m_volHnd != INVALID_HANDLE_VALUE);
}

// ------------------------------------------------------------------------------------------------
void Ntfs::CloseDrive()
{
    if (m_volHnd != INVALID_HANDLE_VALUE) 
    {
        CloseHandle(m_volHnd);	
        m_volHnd = INVALID_HANDLE_VALUE;
    }
}

// ------------------------------------------------------------------------------------------------
void Ntfs::SaveLastError(DWORD error) const
{
    if (error == 0)
        error = GetLastError();
    else if (error == STATUS_INVALID_PARAMETER) {
        m_errorMsg = L"[0xd]The data is invalid.";
        return;
    }

    error &= 0x0ffffff;

    wchar_t* lpMsgBuf;
    FormatMessage( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_ENGLISH_AUS),
        (LPTSTR)&lpMsgBuf,
        0,
        NULL );

    wchar_t msg[MAX_PATH];
    wsprintf(msg, TEXT("[0x%x]%s"), error, (TCHAR*)lpMsgBuf);
    LocalFree(lpMsgBuf );
    m_errorMsg = msg;
}

// ------------------------------------------------------------------------------------------------
bool Ntfs::HasJournal() const
{
    USN_JOURNAL_DATA usnJournalData;
    return QueryJournal(usnJournalData);
}


// ------------------------------------------------------------------------------------------------
bool Ntfs::GetJournal(HandleRecordCb handleCb, void* cbData, USN startUsn, DWORD filter, bool getFileLength, bool getFullPath)
{
    USN_JOURNAL_DATA usnJournalData;

    if (!QueryJournal(usnJournalData))
        return false;
    
    SetFilter(filter == 0 ? sDefaultFilter : filter);
	m_nextUsn = (startUsn == 0) ? usnJournalData.FirstUsn : startUsn;

    while (ReadJournal(
        m_nextUsn,
        usnJournalData.UsnJournalID,
        handleCb, 
        cbData,
        NULL,
        getFileLength, getFullPath))
    {
        // usn is populated with USN after records processed.
    }

    return true;
}

// ------------------------------------------------------------------------------------------------
bool Ntfs::GetJournal(JournalList& list, USN startUsn, DWORD filter, bool getFileLength, bool getFullPath)
{
	USN_JOURNAL_DATA usnJournalData;

    if (!QueryJournal(usnJournalData))
        return false;
    
    SetFilter(filter == 0 ? sDefaultFilter : filter);
	m_nextUsn = (startUsn == 0) ? usnJournalData.FirstUsn : startUsn;

    while (ReadJournal(
        m_nextUsn,
        usnJournalData.UsnJournalID,
        NULL, NULL,
        &list, getFileLength, getFullPath))
    {
        // usn is populated with USN after records processed.
    }

    return true;
}

// ------------------------------------------------------------------------------------------------
void Ntfs::SetFilter(UsnFilter filter)
{
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

    m_filter = filter;
}

// ------------------------------------------------------------------------------------------------
bool Ntfs::QueryJournal(USN_JOURNAL_DATA& usnJournalData) const
{
    DWORD cb;

    /* 
    USN_JOURNAL_DATA_V0  v0;
    USN_JOURNAL_DATA_V1  v1;
    USN_JOURNAL_DATA_V2  v2;

    bool ok0 = (TRUE == DeviceIoControl(m_hnd, FSCTL_QUERY_USN_JOURNAL, NULL, 0,
        &v0, sizeof(v0), &cb, NULL));
    bool ok1 = (TRUE == DeviceIoControl(m_hnd, FSCTL_QUERY_USN_JOURNAL, NULL, 0,
        &v1, sizeof(v1), &cb, NULL));
    bool ok2 = (TRUE == DeviceIoControl(m_hnd, FSCTL_QUERY_USN_JOURNAL, NULL, 0,
        &v2, sizeof(v2), &cb, NULL));
    */

    bool ok = (TRUE == DeviceIoControl(m_volHnd, FSCTL_QUERY_USN_JOURNAL, NULL, 0, 
        &usnJournalData, sizeof(usnJournalData), &cb, NULL));

    if (!ok) 
         SaveLastError();

    return ok;
}

// ------------------------------------------------------------------------------------------------
bool Ntfs::ReadJournal(
        USN& usn,               // usn to start reading, on exit set to usn to resume reading.
        DWORDLONG UsnJournalID, 
        HandleRecordCb handleCb,
        void* cbData,
        JournalList* pList, 
        bool getFileLength,
        bool getFullPath,
        unsigned maxRecords)
{
    BOOL retval = TRUE;

    DWORD bytesRead;
    JournalRecord record;

    m_buffer.resize(sizeof(USN) + sizeof(USN_RECORD) * maxRecords);
    byte* pBuffer = &m_buffer[0];

    /*
    *      READ_USN_JOURNAL_DATA
                USN StartUsn;
                DWORD ReasonMask;
                DWORD ReturnOnlyOnClose;
                DWORDLONG Timeout;
                DWORDLONG BytesToWaitFor;
                DWORDLONG UsnJournalID;
                WORD   MinMajorVersion;
                WORD   MaxMajorVersion;
    */
    READ_USN_JOURNAL_DATA usnData;
    ZeroMemory(&usnData, sizeof(usnData));
    usnData.StartUsn        = usn;		
    usnData.ReasonMask      = m_filter;	
    usnData.UsnJournalID    = UsnJournalID; 
    usnData.BytesToWaitFor  = 0;    // m_buffer.capacity();
    usnData.MaxMajorVersion = 2;

    // Get some records from the journal
    retval = DeviceIoControl(m_volHnd, FSCTL_READ_USN_JOURNAL, &usnData, sizeof(usnData), 
            pBuffer, (DWORD)m_buffer.capacity(), &bytesRead, NULL);

    // We are finished if DeviceIoControl fails, or the number of bytes
    // returned is < sizeof(USN).  
    if (!retval || bytesRead <= sizeof(USN)) 
    {
        SaveLastError();
        return false;
    }

    GetInfo getFileInfo = (getFileLength ? eGetLength : eGetPath);

    // Pass USN to resume reading to caller.
    usn = *(USN*)pBuffer;

    // The first returned record is just after the first sizeof(USN) bytes
    USN_RECORD* pUsnRecord = (PUSN_RECORD) (pBuffer + sizeof(USN));

    // Walk the output buffer
    while ((PBYTE) pUsnRecord < (pBuffer + bytesRead)) 
    {
        LPWSTR pszFileName = (LPWSTR)((PBYTE) pUsnRecord  + pUsnRecord->FileNameOffset);
        // Create a zero terminated copy of the filename
        WCHAR szFile[MAX_PATH];
        int cFileName = pUsnRecord->FileNameLength / sizeof(WCHAR);
        wcsncpy_s(szFile, MAX_PATH, pszFileName, cFileName);
        szFile[cFileName] = 0;

        /*
        	    USN				m_usn;
	            DWORD			m_reason;
	            DWORDLONG		m_fileId;
	            LARGE_INTEGER	m_timestamp;
                LARGE_INTEGER   m_length;
                DWORD           m_fileAttr;
	            wstring			m_filename;
        */
        record.m_filename.clear();
        record.m_usn        = pUsnRecord->Usn;
        record.m_reason     = pUsnRecord->Reason;
        record.m_fileId     = pUsnRecord->FileReferenceNumber;
        record.m_timestamp  = pUsnRecord->TimeStamp;
        record.m_fileAttr   = pUsnRecord->FileAttributes;
        record.m_length.QuadPart = 0;

        if (getFullPath || getFileLength) 
        { 
            if ((record.m_fileAttr & FILE_ATTRIBUTE_DIRECTORY)) 
            {
                if (GetDirInfo(pUsnRecord->ParentFileReferenceNumber, record.m_filename)) {
                    record.m_filename = record.m_filename + sSlashStr + szFile;
                    record.m_length.QuadPart = 0;   // TODO - populate file length !
                } else {
                    record.m_filename = szFile;
                }
            }
            else if (!GetFileInfo(pUsnRecord->FileReferenceNumber, getFileInfo, record.m_filename, record.m_length))
            {
                // record.m_filename = L"?\\";
                // record.m_filename = L"";
                record.m_filename = szFile;
            }
        } 
        else 
        {
            record.m_filename = szFile;
        }

        const DWORD eDirectory = 0x10;
        // if (isDir)
        //    record.m_fileAttr |= eDirectory;

        if ((eDirectory & record.m_fileAttr) != 0)
            record.m_filename += sSlashStr;

        if (pList != NULL)
            pList->push_back(record);
        if (handleCb != NULL)
            handleCb(record, cbData);

        // Move to next record
        pUsnRecord = (PUSN_RECORD)  ((PBYTE) pUsnRecord + pUsnRecord->RecordLength);
    }

    return true;        // TODO - return false if less than 100 record meaning done, else true meaning more data.
}

// ------------------------------------------------------------------------------------------------
const wchar_t* Ntfs::GetReasonString(DWORD dwReason,  std::wstring& outReasonStr) 
{
    // This function converts reason codes into a human readable form
    static const wchar_t* sReasons[] = 
    {
        L"DataOverwrite",         // 0x00000001
        L"DataExtend",            // 0x00000002
        L"DataTruncation",        // 0x00000004
        L"0x00000008",            // 0x00000008
        L"NamedDataOverwrite",    // 0x00000010
        L"NamedDataExtend",       // 0x00000020
        L"NamedDataTruncation",   // 0x00000040
        L"0x00000080",            // 0x00000080
        L"FileCreate",            // 0x00000100
        L"FileDelete",            // 0x00000200
        L"PropertyChange",        // 0x00000400
        L"SecurityChange",        // 0x00000800
        L"RenameOldName",         // 0x00001000
        L"RenameNewName",         // 0x00002000
        L"IndexableChange",       // 0x00004000
        L"BasicInfoChange",       // 0x00008000
        L"HardLinkChange",        // 0x00010000
        L"CompressionChange",     // 0x00020000
        L"EncryptionChange",      // 0x00040000
        L"ObjectIdChange",        // 0x00080000
        L"ReparsePointChange",    // 0x00100000
        L"StreamChange",          // 0x00200000
        L"0x00400000",            // 0x00400000
        L"0x00800000",            // 0x00800000
        L"0x01000000",            // 0x01000000
        L"0x02000000",            // 0x02000000
        L"0x04000000",            // 0x04000000
        L"0x08000000",            // 0x08000000
        L"0x10000000",            // 0x10000000
        L"0x20000000",            // 0x20000000
        L"0x40000000",            // 0x40000000
        L""                       // 0x80000000  *Close*
    };

    outReasonStr.clear();
    for (int i = 0; dwReason != 0; dwReason >>= 1, i++) 
    {
        if ((dwReason & 1) == 1) 
        {
            if (!outReasonStr.empty())
                outReasonStr += L"+";
            outReasonStr += sReasons[i];
        }
    }

    return outReasonStr.c_str();
}

// ------------------------------------------------------------------------------------------------
const wchar_t* Ntfs::GetTimestamp(
    const LARGE_INTEGER& timestamp, 
    std::wstring& outDateTimeStr,
    const wchar_t* dateFmt,
    const wchar_t* timeFmt)
{
    SYSTEMTIME systemTime;
    FileTimeToSystemTime((FILETIME*)&timestamp, &systemTime);

    // Convert system time to local time
    SYSTEMTIME localTime;
    SystemTimeToTzSpecificLocalTime(NULL, &systemTime, &localTime);

    const int sMaxTimeLen = 64;
    wchar_t dateTimeStr[sMaxTimeLen];

    // Get formatted date.
    // D	Day of month as digits with no leading zero for single-digit days.
    // Dd	Day of month as digits with leading zero for single-digit days.
    // Ddd	Day of week as a three-letter abbreviation. The function uses the 
    //      LOCALE_SABBREVDAYNAME value associated with the specified locale.
    // Dddd	Day of week as its full name. The function uses the LOCALE_SDAYNAME 
    //      value associated with the specified locale.
    // M	Month as digits with no leading zero for single-digit months.
    // MM	Month as digits with leading zero for single-digit months.
    // MMM	Month as a three-letter abbreviation. The function uses the 
    //      LOCALE_SABBREVMONTHNAME value associated with the specified locale.
    // MMMM	Month as its full name. The function uses the LOCALE_SMONTHNAME 
    //      value associated with the specified locale.
    // y	Year as last two digits, with a leading zero for years less than 10. 
    //      The same format as "yy."
    // yy	Year as last two digits, with a leading zero for years less than 10.
    // yyyy	Year represented by full four digits.
    // gg	Period/era string. The function uses the CAL_SERASTRING value 
    //      associated with the specified locale. This element is ignored if the date 
    //      to be formatted does not have an associated era or period string.
    int cch = GetDateFormatW(
            (MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_AUS), SORT_DEFAULT)), 
            0,  // DATE_SHORTDATE, 
            &localTime, 
            dateFmt,
            dateTimeStr, sMaxTimeLen);
    dateTimeStr[cch] = 0;
    DWORD err = GetLastError();

    outDateTimeStr = dateTimeStr;
    outDateTimeStr += L" ";

    // Get formatted time
    //  h   Hours with no leading zero for single-digit hours; 12-hour clock
    //  hh	Hours with leading zero for single-digit hours; 12-hour clock
    //  H	Hours with no leading zero for single-digit hours; 24-hour clock
    //  HH	Hours with leading zero for single-digit hours; 24-hour clock
    //  m	Minutes with no leading zero for single-digit minutes
    //  mm	Minutes with leading zero for single-digit minutes
    //  s	Seconds with no leading zero for single-digit seconds
    //  ss	Seconds with leading zero for single-digit seconds
    //  t	One character time marker string, such as A or P
    //  tt	Multicharacter time marker string, such as AM or PM
    cch = GetTimeFormat(
            LOCALE_USER_DEFAULT, 
            0,
            &localTime, 
            timeFmt, 
            dateTimeStr, sMaxTimeLen);
    dateTimeStr[cch] = 0;
    outDateTimeStr += dateTimeStr;

    return outDateTimeStr.c_str();
}

// ------------------------------------------------------------------------------------------------
// Get File Information for fileId, such as file/folder name
// Optionally get file/folder length
// Optionally get/save to a cache.
bool Ntfs::GetFileInfo(
        DWORDLONG fileId, 
        GetInfo getInfo,
        std::wstring& fullPath, 
        LARGE_INTEGER& allocatedSize)
{
    if ((getInfo & eCacheIt) != 0)
    {
        FileInfoCache::const_iterator iter = m_fileInfoCache.find(fileId);
        if (iter != m_fileInfoCache.end())
        {
            fullPath = iter->second.filePath;
            allocatedSize = iter->second.allocatedSize;
            return true;
        }
    }

    allocatedSize.QuadPart = 0;

    typedef ULONG (__stdcall *pNtCreateFile)
    (
        PHANDLE FileHandle,
        ULONG DesiredAccess,
        PVOID ObjectAttributes,
        PVOID IoStatusBlock,
        PLARGE_INTEGER AllocationSize,
        ULONG FileAttributes,
        ULONG ShareAccess,
        ULONG CreateDisposition,
        ULONG CreateOptions,
        PVOID EaBuffer,
        ULONG EaLength
    );

    static pNtCreateFile NtCreatefile = (pNtCreateFile)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtCreateFile");

    /*
        UNICODE_STRING 
            USHORT Length;
            USHORT MaximumLength;
            PWSTR  Buffer;
    */
    USHORT szFileId = sizeof(fileId);   // ex: 8
    UNICODE_STRING fidstr = {szFileId, szFileId, (PWSTR) &fileId};


    /*  OBJECT_ATTRIBUTES
            ULONG Length;                   // sizeof(OBJECT_ATTRIBUTES)
            HANDLE RootDirectory;           // m_volHnd.m_handle
            PUNICODE_STRING ObjectName;     // &fidstr
            ULONG Attributes;               // OBJ_CASE_INSENSITIVE
            PVOID SecurityDescriptor;       // NULL
            PVOID SecurityQualityOfService; // NULL
    */
    OBJECT_ATTRIBUTES objAttr = { sizeof(OBJECT_ATTRIBUTES),  m_volHnd.m_handle, &fidstr, OBJ_CASE_INSENSITIVE, NULL, NULL };
 

    IO_STATUS_BLOCK iosb;
    ZeroMemory(&iosb, sizeof(iosb));
    HANDLE fHnd;

    ULONG status = NtCreatefile(
            &fHnd,                      // FileHandle,
            FILE_READ_ATTRIBUTES,       // DesiredAccess,
            &objAttr,                   // ObjectAttributes,
            &iosb,                      // IoStatusBlock,
            NULL,                       // Allocated Size,
            FILE_ATTRIBUTE_NORMAL,      // FileAttributes
            FILE_SHARE_READ | FILE_SHARE_WRITE,  // ShareAccess,
            FILE_OPEN,                  // CreateDisposition,   
            FILE_OPEN_BY_FILE_ID,       // CreateOptions  
            NULL,                       // EaBuffer,                      
            0);                         // EaLength                      
    
    if (NT_ERROR(status))
    {
        // STATUS_INVALID_PARAMETER    0xC000000DL    
        SaveLastError(status);      
        return false;
    }

    struct FileName 
    {
        DWORD   length;
        wchar_t name[MAX_PATH];
    } fileName;

    bool result;

    
    if (0 == GetFileInformationByHandleEx(fHnd, FileNameInfo, &fileName, sizeof(fileName)))
    {
        SaveLastError();
        result = false;
    }
    else
    {
        fileName.name[fileName.length/sizeof(wchar_t)] = 0;
        fullPath = fileName.name;
        result = true;

        if ((getInfo & eGetLength) != 0)
        {
            FILE_STANDARD_INFO standardInfo;
            if (0 != GetFileInformationByHandleEx(
                    fHnd, FileStandardInfo, &standardInfo, sizeof(standardInfo)))
            {
                if (fullPath == L"\\car")
                    std::wcout << "car\n";
                allocatedSize = standardInfo.AllocationSize;
            }
        }

        if ((getInfo & eCacheIt) != 0)
        {
            m_fileInfoCache[fileId] = InfoCache(fullPath, allocatedSize);
        }
    }

    CloseHandle(fHnd);

    return result;
}
