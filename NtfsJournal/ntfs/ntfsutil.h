
#pragma once

#include "ntfs.h"
#include "fsfilter.h"

struct ReportCfg {
    ReportCfg() :
        startUsn(0), reasonFilter(0), showDetail(false), showFilter(eShowAll),
        usn(false),
        modifyTime(false), size(false), attribute(false),
        directory(true), name(true),
        reason(false), reasonMergeAll(false),
        getFileLength(false),
        getFullPath(true),
        slash('\\'), fmtChr('%'), dirAttr(L"D"), separator(L" "),
        dateFmt(L"dd-MMM-yyyy"), timeFmt(L"HH:mm"),
        outputFmt(NULL) { }

    MultiFilter<JRecord> filter;
    DWORD64         startUsn;
    DWORD           reasonFilter;
    bool            showDetail;
    enum ShowFilter {
        eShowAll, eShowDir, eShowFile
    };
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
    bool            getFileLength;     // true get file length (expensive time to get value)
    bool            getFullPath;       // true get file/dir full path (expensive time to get value)

    // Output controls
    wchar_t         slash;
    wchar_t         fmtChr;
    const wchar_t*  dirAttr;

    const wchar_t*  separator;
    std::wstring    dateFmt;
    std::wstring    timeFmt;
    const wchar_t*  outputFmt;
};

namespace Ntfs_Journal {
    DWORD ParseReason(const wchar_t* reasons);

    int ListJournal(const wchar_t* drivePath, Ntfs& ntfs, ReportCfg& cfg);

    bool ReadRegistry(const wchar_t* keyStr, std::wstring& valueStr);
    bool ReadRegistry(wchar_t drive, DWORD64& nextUsn);
    void WriteRegistry(wchar_t drive, DWORD64 nextUsn);
}