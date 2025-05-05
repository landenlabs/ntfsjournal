

#include "ntfsutil.h"
#include "localefmt.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <set>


namespace Ntfs_Journal {
// ------------------------------------------------------------------------------------------------
DWORD ParseReason(const wchar_t* reasons) {
    static
        struct {
        wchar_t* reasonStr;
        DWORD    filter;
    }
    sReasons[] =
    {
        { L"overwrite", USN_REASON_DATA_OVERWRITE },
        { L"extend",    USN_REASON_DATA_EXTEND },
        { L"truncate",  USN_REASON_DATA_TRUNCATION },
        { L"create",    USN_REASON_FILE_CREATE },
        { L"delete",    USN_REASON_FILE_DELETE },
        { L"rename",    USN_REASON_RENAME_OLD_NAME | USN_REASON_RENAME_NEW_NAME },
        { L"security",  USN_REASON_SECURITY_CHANGE },
        { L"basic",     USN_REASON_BASIC_INFO_CHANGE },
        { L"link",      USN_REASON_HARD_LINK_CHANGE | USN_REASON_REPARSE_POINT_CHANGE },
        { L"all", ~(DWORD)0 }
    };

    DWORD filter = 0;
    for (unsigned idx = 0; idx < ARRAYSIZE(sReasons); idx++) {
        if (wcsstr(reasons, sReasons[idx].reasonStr) != NULL)
            filter |= sReasons[idx].filter;
    }

    if (filter == 0)
        std::wcerr << L"No Reason filters found for:" << reasons << std::endl;
    return filter;
}

// ------------------------------------------------------------------------------------------------
// Format output using special meta strings which start with %
//      %t=time, %s=size, %r=reason
//      %p=path(dir+filename), %c=drive, %d=directory 
//      %f=filename (name+ext), %n=name, %e=extension 
//
// All formats can include a field width to force padding with spaces.
//   %20f  will output the filename in 20 characters or more.

void FormatOutput(ReportCfg& cfg, Ntfs::JournalRecord& jRec) {
    std::wstring dateTimeStr;
    wchar_t str[100];
    std::wstring field;
    std::wstring reasonStr;
    int pos;
    DWORD reasonMask = 0xffffffff;

    Ntfs::GetTimestamp(jRec.m_timestamp, dateTimeStr, cfg.dateFmt.c_str(), cfg.timeFmt.c_str());

    for (const wchar_t* pFmt = cfg.outputFmt; *pFmt != 0; pFmt++) {
#ifdef BACKSLASH_SPECIAL
        if (*pFmt == '\\') {
            switch (*++pFmt) {
            case 'n':
                std::wcout << "\n";
                break;
            case 'r':
                std::wcout << "\r";
                break;
            case 't':
                std::wcout << "\t";
                break;
            case 'x':
            {
                wchar_t* endPtr;
                long n = wcstol(pFmt, &endPtr, 16);
                if (endPtr != pFmt) {
                    std::wcout << (char)n;
                    pFmt = endPtr - 1;
                }
            }
            break;
            default:
                std::wcout << *pFmt;
                break;
            }
        } else
#endif
            if (*pFmt == cfg.fmtChr) {
                pFmt++;

                if (*pFmt == NULL)
                    return;
                else if (*pFmt == cfg.fmtChr)
                    std::wcout << cfg.fmtChr;
                else {
                    wchar_t* endPtr;
                    long fieldWidth = wcstol(pFmt, &endPtr, 10);
                    if (endPtr == pFmt)
                        fieldWidth = 0;
                    pFmt = endPtr;
                    switch (*pFmt) {
                    case 'a':    // attribute
                    {
                        std::wstring attributes;
                        if (eDirectory & jRec.m_fileAttr) attributes += cfg.dirAttr;
                        if (eSystem & jRec.m_fileAttr) attributes += L"S";
                        if (eHidden & jRec.m_fileAttr) attributes += L"H";
                        if (eReadOnly & jRec.m_fileAttr) attributes += L"R";
                        std::wcout << std::setw(4) << attributes;
                    }
                    break;
                    case 't':
                        field = dateTimeStr;
                        break;
                    case 's':
                        std::wcout << LocaleFmt::snprintf(str, ARRAYSIZE(str), L"%*lld", fieldWidth, jRec.m_length.QuadPart);
                        break;
                    case 'r':    // reason
                        if ((jRec.m_reason & 0xf00) != 0)
                            reasonMask &= ~0xff;    // suppress the extend and overwrite if Create or Delete.
                        field = Ntfs::GetReasonString(jRec.m_reason & reasonMask, reasonStr);
                        break;
                    case 'p':    // path = directory + name
                        field = jRec.m_filename;
                        break;
                    case 'd':    // directory
                        field = jRec.m_filename.substr(0, jRec.m_filename.find_last_of(cfg.slash) + 1);
                        if (!field.empty())
                            field.erase(field.end() - 1);
                        break;
                    case 'f': // filename = name + ext
                        field = jRec.m_filename.substr(jRec.m_filename.find_last_of(cfg.slash) + 1);
                        break;
                    case 'n': // name
                        field = jRec.m_filename.substr(jRec.m_filename.find_last_of(cfg.slash) + 1);
                        field = field.substr(0, field.find_last_of('.'));
                        break;
                    case 'e': // ext
                        pos = (int)jRec.m_filename.find_last_of('.');
                        field = (pos >= 0 ? jRec.m_filename.substr(pos + 1) : L"");
                        break;
                    default:
                        std::wcout << *pFmt;
                        continue;
                    }

                    while ((long)field.length() < fieldWidth)
                        field += ' ';
                    std::wcout << field;
                }
            } else {
                std::wcout << *pFmt;
            }
    }
#ifndef BACKSLASH_SPECIAL
    std::wcout << std::endl;
#endif
}

typedef std::set<size_t> DeletedSet;
static DeletedSet sDeletedSet;

// ------------------------------------------------------------------------------------------------
void HandleRecordCb(Ntfs::JournalRecord& jRec, void* cbData) {
    ReportCfg& cfg = *(ReportCfg*)cbData;
    wchar_t str[30];

    if (jRec.m_filename.length() != 0 && cfg.filter.IsMatch(jRec, &cfg)) {
        // TODO - move this logic into a Filter.
        if (cfg.showFilter != ReportCfg::eShowAll) {
            bool isDir = (jRec.m_fileAttr & eDirectory) != 0;
            bool showDir = cfg.showFilter == ReportCfg::eShowDir;
            if (showDir != isDir)
                return;
        }

        if (!cfg.showDetail) {
            if ((jRec.m_reason & USN_REASON_FILE_DELETE) != 0) {
                // Delete entries can be duplicates because their fileId will be different even
                // for the exact same filename.
                DeletedSet::iterator iter = sDeletedSet.find(std::hash<std::wstring>{}(jRec.m_filename));
                if (iter == sDeletedSet.end())
                    sDeletedSet.insert(std::hash<std::wstring>{}(jRec.m_filename));
                else
                    return;
            }
        }

        if (cfg.outputFmt != NULL) {
            FormatOutput(cfg, jRec);
            return;
        }

        std::wstring dateTimeStr;
        std::wstring reasonStr;

        if (cfg.usn)
            std::wcout << std::setw(15) << jRec.m_usn
            << cfg.separator;

        if (cfg.modifyTime)
            std::wcout
            << Ntfs::GetTimestamp(jRec.m_timestamp, dateTimeStr, cfg.dateFmt.c_str(), cfg.timeFmt.c_str())
            << cfg.separator;
        if (cfg.size)
            std::wcout
            << std::setw(15)
            << LocaleFmt::snprintf(str, ARRAYSIZE(str), L"%lld", jRec.m_length.QuadPart)
            << cfg.separator;

        if (cfg.attribute) {
            std::wstring attributes;
            if (eDirectory & jRec.m_fileAttr) attributes += cfg.dirAttr;
            if (eSystem & jRec.m_fileAttr) attributes += L"S";
            if (eHidden & jRec.m_fileAttr) attributes += L"H";
            if (eReadOnly & jRec.m_fileAttr) attributes += L"R";
            std::wcout << std::setw(4) << attributes << cfg.separator;
        }

        int namePos = (int)jRec.m_filename.find_last_of(cfg.slash);
        const wchar_t* name = ((namePos > 0) ? jRec.m_filename.c_str() + namePos + 1 : jRec.m_filename.c_str());
        if (cfg.directory)
            std::wcout << jRec.m_filename;
        else
            std::wcout << name;

        if (cfg.reason)
            std::wcout << cfg.separator << Ntfs::GetReasonString(jRec.m_reason, reasonStr);
        std::wcout << std::endl;
    }
}

typedef  std::map<DWORD64, Ntfs::JournalRecord> JournalMap;
static JournalMap sJournalMap;

// ------------------------------------------------------------------------------------------------
void HandleDupRecordCb(Ntfs::JournalRecord& jRec, void* cbData) {
    ReportCfg& cfg = *(ReportCfg*)cbData;

    if (!jRec.m_filename.empty() && cfg.filter.IsMatch(jRec, &cfg)) {
        // TODO - move this logic into a Filter.
        if (cfg.showFilter != ReportCfg::eShowAll) {
            bool isDir = (jRec.m_fileAttr & eDirectory) != 0;
            bool showDir = cfg.showFilter == ReportCfg::eShowDir;
            if (showDir != isDir)
                return;
        }

        JournalMap::iterator iter = sJournalMap.find(jRec.m_fileId);

        if (iter == sJournalMap.end()) {
            sJournalMap[jRec.m_fileId] = jRec;
        } else {
            DWORD reason = jRec.m_reason | iter->second.m_reason;
            sJournalMap[jRec.m_fileId] = jRec;
            if (cfg.reasonMergeAll)
                sJournalMap[jRec.m_fileId].m_reason = reason;
        }
    }
}

// ------------------------------------------------------------------------------------------------
// List NTFS journal, return -1 on error or 1 on success.  

int ListJournal(const wchar_t* drivePath, Ntfs& ntfs, ReportCfg& cfg) {
    if (!ntfs.OpenDrive(*drivePath)) {
        std::wcerr << "Failed to open drive:" << drivePath << "\nError:" << ntfs.GetLastErrorMsg() << std::endl;
        return -1;
    }

    if (!ntfs.HasJournal()) {
        std::wcerr << "Journal not available on drive:" << drivePath << std::endl;
        return -1;
    }

    bool status;
    if (cfg.showDetail) {
        status = ntfs.GetJournal(HandleRecordCb, &cfg, cfg.startUsn, cfg.reasonFilter, cfg.getFileLength, cfg.getFullPath);
    } else {
        status = ntfs.GetJournal(HandleDupRecordCb, &cfg, cfg.startUsn, cfg.reasonFilter, cfg.getFileLength, cfg.getFullPath);
        for (JournalMap::iterator iter = sJournalMap.begin();
            iter != sJournalMap.end();
            iter++) {
            HandleRecordCb(iter->second, &cfg);
        }
    }

    return status ? 1 : -1;
}



const wchar_t sRegKeyStr[] = L"SOFTWARE\\NtfsJournal";
wchar_t gRegName[] = L"NextUsn-X";
const unsigned sRegNameDriveOffset = 8;

// ------------------------------------------------------------------------------------------------

bool ReadRegistry(const wchar_t* keyStr, std::wstring& valueStr) {
    // Load any previously stored registry values.
    HKEY hKey;
    bool okay = false;
    if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE, sRegKeyStr,
        0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL,
        &hKey, NULL)) {
        const int sMaxStrLen = 256;
        wchar_t value[sMaxStrLen];
        DWORD dataLen = sMaxStrLen;
        okay = (ERROR_SUCCESS == RegQueryValueEx(hKey, keyStr, NULL, NULL, (LPBYTE)value, &dataLen));
        DWORD error = GetLastError();
        RegCloseKey(hKey);

        if (okay) {
            value[dataLen] = NULL;
            valueStr = value;
        }
    }

    DWORD error = GetLastError();
    return okay;
}

// ------------------------------------------------------------------------------------------------

bool ReadRegistry(wchar_t drive, DWORD64& nextUsn) {
    // Load any previously stored registry values.
    HKEY hKey;
    nextUsn = 0;
    bool okay = false;
    if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE, sRegKeyStr,
        0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL,
        &hKey, NULL)) {
        DWORD dataLen = sizeof(nextUsn);
        gRegName[sRegNameDriveOffset] = drive;
        okay = (ERROR_SUCCESS == RegQueryValueEx(hKey, gRegName, NULL, NULL, (LPBYTE)&nextUsn, &dataLen));
        DWORD error = GetLastError();
        RegCloseKey(hKey);
    }
    DWORD error = GetLastError();
    return okay;
}

// ------------------------------------------------------------------------------------------------
void WriteRegistry(wchar_t drive, DWORD64 nextUsn) {
    HKEY hKey;
    if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE, sRegKeyStr,
        0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL,
        &hKey, NULL)) {
        gRegName[sRegNameDriveOffset] = drive;
        RegSetValueEx(hKey, gRegName, 0, REG_QWORD, (const BYTE*)&nextUsn, sizeof(nextUsn));
        RegCloseKey(hKey);
    }
}

}