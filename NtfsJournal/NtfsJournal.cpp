// ------------------------------------------------------------------------------------------------
// List Windows NTFS Journal.
//
// Author:  Dennis Lang   July-2011
// http://home.comcast.net/~lang.dennis/
// ------------------------------------------------------------------------------------------------

#include <iostream>
#include <iomanip>
#include <math.h>
#include <set>
#include <functional>       // for std::hash

#include "Ntfs.h"
#include "Support\GetOpts.h"
#include "Support\FsFilter.h"
#include "Support\LocaleFmt.h"

#define _VERSION "v3.0"

char sUsage[] =
    "\n"
    "Ntfs Journal  " _VERSION " - " __DATE__ "\n"
    "By: Dennis Lang\n"
    "https://home.comcast.net/~lang.dennis/\n"
    "\n"
    "Description:\n"
    "  List NTFS Journal which tracks recent file/folder changes.\n"
    "  Use 'fsutil usn ...' to create and configure NTFS journal.\n"
    "Use:\n"
    "   NtfsJournal [options] <localNTFSdrive>... \n"
    " Filter (see examples below):\n"
    "   -a [d|f]                  ; Just Directories or Files, default is both \n"
    "   -d                        ; Show detail, by default remove duplicates\n"
    "   -f <findFilter>           ; Filter by file path, use * or ? patterns \n"
    "   -g <findFilter>           ; Filter by file path, using grep reqular Expression ^[]+*.$ \n"
    "   -r <changeReasonFilter>   ; Filter by change flags \n"
    "   -s <size>                 ; Filter by file size  \n"
    "   -t <relativeModifyDate>   ; Filter by Modify Time, value is relative days \n"
    "   -u <usn>                  ; Start scan with usn number, see -U\n"
    "   -u -                      ; Start with previously stored USN in registry\n"
    "                             ; On exit, last USN is automatically stored in registry\n"
    " Report (what appears in output):\n"
    "   -A                        ; Include attributes \n"
    "   -B <dirAttr>              ; Change directory attribute 'D' to some other string \n"
    "   -C <fmtChar>              ; Change format character '%' to some other character \n"
    "   -D                        ; Disable directory \n"
    "   -F <fmt>                  ; Format output, %t=time, %s=size, %r=reason,%a=attribute \n"
    "                             ; %p=path(dir+filename), %c=drive, %d=directory,\n"
    "                             ; %f=filename (name+ext), %n=name, %e=extension\n"
    "                             ; Field can be padded, as in %10s %15t %20f\n"
    "   -R [a|l]                  ; Include Reasons, All or just Last, default is just Last\n"
    "   -S                        ; Include size \n"
    "   -T                        ; Include modify time \n"
    "   -U                        ; Include USN number \n"
    "\n"
    " Registry:\n"
    "   HKEY_LOCAL_MACHINE\\SOFTWARE\\NtfsJournal \n"
    "       TimeFormat  string   HH:mm          ; google 'msdn GetTimeFormat' \n"
    "       DateFormat  string   dd-MMM-yyyy    ; google 'msdn GetDateFormat' \n"
    "\n"
    " Examples:\n"
    "  No filtering:\n"
    "    c:                 ; scan c drive, display filenames. \n"
    "    -TSA c:            ; scan c drive, display  time, size, attributes. \n"
    "  Filter examples (precede 'f' command letter with ! to invert rule):\n"
    "    -f *.txt d:        ; files ending in .txt on d: drive \n"
    "    -!f *.txt d:       ; files NOT ending in .txt on d: drive \n" 
    "    -f *.txt -!f \\$RECY* d:  ; files ending in .txt but not in recyle.bin on d: drive \n" 
    "    -f F* c: d:        ; limit scan to files starting with F on either C or D \n"
    "\n"
    "  Alternate using grep regular expression, note double backslash for every directory slash \n"
    "  Also recommended you place the pattern inside quotations  \n"
    "    -g \"\\\\tmp\\\\sub\\\\[^\\\\]+\"   d:  ; files inside \\tmp\\sub\\ but nothing deeper \n"
    "  The above is similar to -f \\tmp\\sub\\* except the -g version is anchored on the left \n"
    "  and must match starting with its first character. \n" 
    "\n"
    "  Time and size options:\n"
    "    -t 2.5 -f *.log    ; modified more than 2.5 days ago and ending in .log on c drive \n"
    "    -t -7 e:           ; modified less than 7 days ago on e drive \n"
    "    -s 1000 d:         ; size more than 1000 bytes on d drive \n"
    "    -s -1000 d: e:     ; size less than 1000 bytes on d and e drive \n"
    "                       ; *** NOTE: Size is rarely populated due to performance\n"
    "    -F \"%20t %20s %40p\"  c: ; Format output\n"
    "    -C # -F \"#t,#s,#p\"  c:  ; Change format character, and format output\n"
    "    -F \"copy %p \\\\remote\\d$\\data\\%f\" d:\\data\\* > sync.bat\n"
    "\n"
    "  Filter Reasons Keywords:\n"
    "       all, \n"
    "       overwrite, extend, truncate\n"
    "       create, delete, rename\n"
    "       security, basic, link\n"
    "  Examples:\n"
    "     -r overwrite+extend+truncate  ; File content changes\n"
    "     -r create+delete+rename       ; File life changes\n"
    "   Defaults is:  overwrite+extend+truncate+create+delete+rename\n"
    ;

const wchar_t sRegKeyStr[] = L"SOFTWARE\\NtfsJournal";
wchar_t gRegName[] = L"NextUsn-X";
const unsigned sRegNameDriveOffset = 8;

enum FileAttributeBits
{
    eCompressed = 0x00000800,
    eArchive    = 0x00000020,
    eDirectory  = 0x00000010,
    eSystem     = 0x00000004, 
    eHidden     = 0x00000002, 
    eReadOnly   = 0x00000001 
};


struct ReportCfg
{
    ReportCfg() : 
            startUsn(0), reasonFilter(0), showDetail(false), showFilter(eShowAll),
            usn(false), 
            modifyTime(false), size(false),  attribute(false), 
            directory(true), name(true),
            reason(false), reasonMergeAll(false),
            slash('\\'), fmtChr('%'), dirAttr(L"D"), separator(L" "),
            dateFmt(L"dd-MMM-yyyy"), timeFmt(L"HH:mm"),
            outputFmt(NULL)
        { }

        MultiFilter<JRecord> filter;
        DWORD64         startUsn;
        DWORD           reasonFilter;
        bool            showDetail;
        enum ShowFilter { eShowAll, eShowDir, eShowFile };
        ShowFilter      showFilter;

        // File scan report columns
        bool            usn;
        bool            modifyTime;        // include modify time
        bool            size;
        bool            attribute;
        bool            directory;         // include full directory path
        bool            name;
        bool            reason;
        bool            reasonMergeAll;    // true = merge reasons when removing duplicates.
                                           // false = keep last reason (newest)

        // Output controls
        wchar_t         slash;
        wchar_t         fmtChr;
        const wchar_t*  dirAttr;    

        const wchar_t*  separator;
        std::wstring    dateFmt;
        std::wstring    timeFmt;
        const wchar_t*  outputFmt;
};

// ------------------------------------------------------------------------------------------------
DWORD ParseReason(const wchar_t* reasons)
{
    static 
    struct 
    {
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
        { L"all", ~0 }
    };

    DWORD filter = 0;
    for (unsigned idx = 0; idx < ARRAYSIZE(sReasons); idx++)
    {
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

void FormatOutput(ReportCfg& cfg, Ntfs::JournalRecord& jRec)
{
    std::wstring dateTimeStr;
    wchar_t str[100];
    std::wstring field;
    std::wstring reasonStr;
    int pos;
    DWORD reasonMask = 0xffffffff;

    Ntfs::GetTimestamp(jRec.m_timestamp, dateTimeStr, cfg.dateFmt.c_str(), cfg.timeFmt.c_str()); 
            
    for (const wchar_t* pFmt = cfg.outputFmt; *pFmt != 0; pFmt++)
    {
#ifdef BACKSLASH_SPECIAL
        if (*pFmt == '\\')
        {
            switch (*++pFmt)
            {
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
                    if (endPtr != pFmt)
                    {
                        std::wcout << (char)n;
                        pFmt = endPtr - 1;
                    }
                }
                break;
            default:
                std::wcout << *pFmt;
                break;
            }
        }
        else 
#endif
        if (*pFmt == cfg.fmtChr)
        {
            pFmt++;

            if (*pFmt == NULL)
               return;
            else if (*pFmt == cfg.fmtChr)
               std::wcout << cfg.fmtChr;
            else
            {
               wchar_t* endPtr;
               long fieldWidth = wcstol(pFmt, &endPtr, 10);
               if (endPtr == pFmt)
                   fieldWidth = 0;
               pFmt = endPtr;
               switch (*pFmt)
               {
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
                        reasonMask &= ~0xff;    // surpress the extend and overwrite if Create or Delete.
                    field = Ntfs::GetReasonString(jRec.m_reason & reasonMask, reasonStr);
                    break;
               case 'p':    // path = directory + name
                   field = jRec.m_filename;
                   break;
               case 'd':    // directory
                   field = jRec.m_filename.substr(0, jRec.m_filename.find_last_of(cfg.slash)+1);
                   if (!field.empty())
                       field.erase(field.end() -1);
                   break;
                case 'f': // filename = name + ext
                    field = jRec.m_filename.substr(jRec.m_filename.find_last_of(cfg.slash)+1);
                    break;
                case 'n': // name
                    field = jRec.m_filename.substr(jRec.m_filename.find_last_of(cfg.slash)+1);
                    field = field.substr(0, field.find_last_of('.'));
                    break;
                case 'e': // ext
                    pos = (int)jRec.m_filename.find_last_of('.');
                    field = (pos >= 0 ? jRec.m_filename.substr(pos+1) : L"");
                    break;
                default:
                   std::wcout << *pFmt;
                   continue;
                }

                while ((long)field.length() < fieldWidth)
                       field += ' ';
                std::wcout << field;
            }
        }
        else
        {
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
void HandleRecordCb(Ntfs::JournalRecord& jRec, void* cbData)
{
    ReportCfg& cfg = *(ReportCfg*)cbData;
    wchar_t str[30];
    
    if (jRec.m_filename.length() != 0 && cfg.filter.IsMatch(jRec, &cfg))
    {
        // TODO - move this logic into a Filter.
        if (cfg.showFilter != ReportCfg::eShowAll)
        {
            bool isDir = (jRec.m_fileAttr & eDirectory) != 0;
            bool showDir = cfg.showFilter == ReportCfg::eShowDir;
            if (showDir != isDir)
                return;
        }

        if (!cfg.showDetail)
        {
            if ((jRec.m_reason & USN_REASON_FILE_DELETE) != 0)
            {
                // Delete entries can be duplicates because their fileId will be different even
                // for the exact same filename.
                DeletedSet::iterator iter = sDeletedSet.find(std::hash<std::wstring>{}(jRec.m_filename));
                if (iter == sDeletedSet.end())
                    sDeletedSet.insert(std::hash<std::wstring>{}(jRec.m_filename));
                else
                    return;
            }
        }

        if (cfg.outputFmt != NULL)
        {
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

        if (cfg.attribute)
        {
            std::wstring attributes;
            if (eDirectory & jRec.m_fileAttr) attributes += cfg.dirAttr;
            if (eSystem & jRec.m_fileAttr) attributes += L"S";
            if (eHidden & jRec.m_fileAttr) attributes += L"H";
            if (eReadOnly & jRec.m_fileAttr) attributes += L"R";
            std::wcout << std::setw(4) << attributes << cfg.separator;
        }

        int namePos = (int)jRec.m_filename.find_last_of(cfg.slash);
        const wchar_t* name = ((namePos > 0) ? jRec.m_filename.c_str() + namePos+1 : jRec.m_filename.c_str());
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
void HandleDupRecordCb(Ntfs::JournalRecord& jRec, void* cbData)
{
    ReportCfg& cfg = *(ReportCfg*)cbData;

    if (!jRec.m_filename.empty() && cfg.filter.IsMatch(jRec, &cfg))
    {
        // TODO - move this logic into a Filter.
        if (cfg.showFilter != ReportCfg::eShowAll)
        {
            bool isDir = (jRec.m_fileAttr & eDirectory) != 0;
            bool showDir = cfg.showFilter == ReportCfg::eShowDir;
            if (showDir != isDir)
                return;
        }

        JournalMap::iterator iter = sJournalMap.find(jRec.m_fileId);
       
        if (iter == sJournalMap.end())
        {
            sJournalMap[jRec.m_fileId] = jRec;
        }
        else
        {
            DWORD reason = jRec.m_reason | iter->second.m_reason;
            sJournalMap[jRec.m_fileId] = jRec;
            if (cfg.reasonMergeAll)
                sJournalMap[jRec.m_fileId].m_reason = reason;
        }
    }
}

// ------------------------------------------------------------------------------------------------
// List NTFS journal, return -1 on error or 1 on success.  

int ListJournal(const wchar_t* drivePath, Ntfs& ntfs, ReportCfg& cfg)
{
    if (!ntfs.OpenDrive(*drivePath))
    {
        std::wcerr << "Failed to open drive:" << drivePath << "\nError:" << ntfs.GetLastErrorMsg() << std::endl;
        return -1;
    }
    
    if (!ntfs.HasJournal())
    {
        std::wcerr << "Journal not available on drive:" << drivePath << std::endl;
        return -1;
    }

    bool status;
    if (cfg.showDetail)
    {
        status = ntfs.GetJournal(HandleRecordCb, &cfg, cfg.startUsn, cfg.reasonFilter);
    }
    else
    {
        status = ntfs.GetJournal(HandleDupRecordCb, &cfg, cfg.startUsn, cfg.reasonFilter);
        for (JournalMap::iterator iter = sJournalMap.begin();
            iter != sJournalMap.end();
            iter++)
        {
            HandleRecordCb(iter->second, &cfg);
        }
    }

    return status ? 1 : -1;
}

// ------------------------------------------------------------------------------------------------

bool ReadRegistry(const wchar_t* keyStr, std::wstring& valueStr)
{
    // Load any previously stored registry values.
    HKEY hKey;
    bool okay = false;
    if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE, sRegKeyStr,
                0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL,
                &hKey, NULL))
    {
        const int sMaxStrLen = 256;
        wchar_t value[sMaxStrLen];
        DWORD dataLen = sMaxStrLen;
        okay = (ERROR_SUCCESS == RegQueryValueEx(hKey, keyStr, NULL, NULL, (LPBYTE)value, &dataLen));
        DWORD error = GetLastError();
        RegCloseKey(hKey);

        if (okay)
        {
            value[dataLen] = NULL;
            valueStr = value;
        }
    }

    DWORD error = GetLastError();
    return okay;
}


// ------------------------------------------------------------------------------------------------

bool ReadRegistry(wchar_t drive, DWORD64& nextUsn)
{
    // Load any previously stored registry values.
    HKEY hKey;
    nextUsn = 0;
    bool okay = false;
    if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE, sRegKeyStr,
                0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL,
                &hKey, NULL))
    {
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
 void WriteRegistry(wchar_t drive, DWORD64 nextUsn)
 {
    HKEY hKey;
    if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE, sRegKeyStr,
                0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL,
                &hKey, NULL))
    {
        gRegName[sRegNameDriveOffset] = drive;
        RegSetValueEx(hKey, gRegName, 0, REG_QWORD, (const BYTE*)&nextUsn, sizeof(nextUsn));
        RegCloseKey(hKey);
    }
}

// ------------------------------------------------------------------------------------------------
// Return negative on error, postive on success and zero if nothing done.

int wmain(int argc, const wchar_t* argv[])
{
    if (argc == 1)
    {
        std::wcout << sUsage;
        return 0;
    }

    bool loadUsnFromReg = false;
    bool matchOn = true;
    ReportCfg cfg;
    Ntfs ntfs;

    // dateFmt(L"dd-MMM-yyyy"), timeFmt(L"HH:mm"),
    ReadRegistry(L"TimeFormat", cfg.timeFmt);    
    ReadRegistry(L"DateFormat", cfg.dateFmt);    

    GetOpts<wchar_t> getOpts(argc, argv, L",!a:df:g:r:s:t:u:AB:C:DF:R:STU?");

    while (getOpts.GetOpt())
    {
        switch (getOpts.Opt())
        {
        case ',':
            cfg.separator = L", ";
            break;

        case '!':   // not (invert match)
            matchOn = false;
            break;

        case 'a':   // show attribute filter, just Files(f) or Directories(d)
            if (getOpts.OptArg() == L"d")    
                cfg.showFilter = ReportCfg::eShowDir;
            else    
                cfg.showFilter = ReportCfg::eShowFile;
            break;

        case 'd':   // show detail
            cfg.showDetail = true;
            break;

        case 'f':   // file filter
            cfg.filter.List().push_back(new MatchName(getOpts.OptArg(), IsNameIcase, matchOn));
            break;

        case 'g':   // grep (regular expression) file filter
            cfg.filter.List().push_back(new MatchName(std::wregex(getOpts.OptArg(), std::regex::icase), IsGrepIcase, matchOn));
            break;

        case 'r':
            cfg.reasonFilter = ParseReason(getOpts.OptArg());
            break;
        case 's':   // size
            {
                wchar_t* endPtr;
                long fileSize = wcstol(getOpts.OptArg(), &endPtr, 10);
                if (endPtr == getOpts.OptArg())
                {
                    std::wcerr << "Invalid Size argument:" << getOpts.OptArg() << std::endl;
                    return -1;
                }
                cfg.filter.List().push_back(new MatchSize(labs(fileSize), fileSize > 0 ? IsSizeGreater : IsSizeLess, matchOn));
            }
            matchOn = true;
            break;

        case 't':   // modify time
            {
                wchar_t* endPtr;
                double days = wcstod(getOpts.OptArg(), &endPtr);
                if (endPtr == getOpts.OptArg())
                {
                    std::wcerr << "Invalid Modify Days argument, expect floating point number\n";
                    return -1;
                }
                FILETIME  daysAgo = FsTime::TodayUTC() - FsTime::TimeSpan::Days(fabs(days));
                cfg.filter.List().push_back(new MatchDate(daysAgo, 
                        days < 0 ? IsDateModifyGreater : IsDateModifyLess, matchOn));
                // std::wcout << "Today      =" << FsTime::TodayUTC() << std::endl;
                // std::wcout << "Filter date=" << daysAgo << std::endl;
            }
            matchOn = true;
            break;

        case 'u':   // usn starting number
            {
            wchar_t* endPtr;
            loadUsnFromReg = false;
            cfg.startUsn = wcstoul(getOpts.OptArg(), &endPtr, 10);
            if (cfg.startUsn == 0 && *endPtr == '-')
                loadUsnFromReg = true;
            }
            break;

        case 'A':   // attributes
            cfg.attribute = !cfg.attribute;
            break;
        case 'B':   // directory attribute, defaults to D
            cfg.dirAttr = getOpts.OptArg();
            break;
        case 'C':   // format Character, defaults to %
            cfg.fmtChr = *getOpts.OptArg();
            break;
        case 'D':   // directory path
            cfg.directory = !cfg.directory;
            break;
        case 'F':   // output format
            cfg.outputFmt = getOpts.OptArg();
            break;
        case 'R':   // Include Reason in report, -R or -Ra or -Rl
            cfg.reason = !cfg.reason;
            cfg.reasonMergeAll = false;
            if (getOpts.OptArg() != NULL)
            {
                const wchar_t* pArg = getOpts.OptArg();
                if (wcslen(pArg) == 1 && (pArg[0] == 'a' || pArg[0] == 'l'))
                    cfg.reasonMergeAll = pArg[0] == 'a';
                else
                    getOpts.NotOurArg();
            }
            break;
        case 'S':   // size
            cfg.size = !cfg.size;
            break;     
        case 'U':   // usn
            cfg.usn = !cfg.usn;
            break;
        case 'T':   // modify time
            cfg.modifyTime = !cfg.modifyTime;
            break;
      
        default:
        case '?':
            std::wcout << sUsage;
            return 0;
        }
    }

    DWORD error = 0;
    if (getOpts.NextIdx() < argc)
    {
        int addedFilter = -1;
        for (int optIdx = getOpts.NextIdx(); error == 0 && optIdx < argc; optIdx++)
        {
            const wchar_t* arg = argv[optIdx];
            if (wcslen(arg) > 2)
            {
                if (addedFilter != -1)
                {
                    cfg.filter.List().erase(cfg.filter.List().end() - 1);
                }
                // Use last argument as both drive selector and as file pattern.
                int off = 0;
                if (arg[1] == ':' && arg[2] != '\\') 
                    off = 2;
                addedFilter = (int)cfg.filter.List().size();
                cfg.filter.List().push_back(new MatchName(arg+off, IsNameIcase, matchOn));
            }

            if (loadUsnFromReg)
                ReadRegistry(arg[0], cfg.startUsn);

            std::wcerr << L"--- Journal for " << arg << std::endl;

            DWORD tick = GetTickCount();
            error |= ListJournal(arg, ntfs, cfg);
            std::wcerr << L"--- " << (GetTickCount() - tick)/1000.0 << L" seconds\n";

            std::wcout << std::endl;
        }
    }

    if (ntfs.IsOpen() && ntfs.GetNextUsn() != 0)
    {
        WriteRegistry(ntfs.GetDrive(), ntfs.GetNextUsn());
    }

	return error;
}

