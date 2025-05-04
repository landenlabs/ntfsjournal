// ------------------------------------------------------------------------------------------------
// List Windows NTFS Journal.
//
// Project: NTFSjournal
// Author:  Dennis Lang   Apr-2011
// https://lanenlabs.com
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

#include <iostream>
#include <iomanip>
#include <math.h>
#include <set>
#include <functional>       // for std::hash

#include "winerrhandlers.h"
using namespace WinErrHandlers;

#include "getopts.h"
#include "fsfilter.h"
#include "localefmt.h"

#include "ntfsutil.h"   // namespace Ntfs_Journal

#define _VERSION "v3.02"

char sUsage[] =
    "\n"
    "Ntfs Journal  " _VERSION " - " __DATE__ "\n"
    "By: Dennis Lang\n"
    "https://landenlabs.com\n"
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
    "   -p                        ; Skip finding full path, much faster results\n"
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
    "    -p c:              ; fast scan c drive, just filenames, not full path. \n"
    "  Filter examples (precede 'f' command letter with ! to invert rule):\n"
    "    -f *.txt d:        ; files ending in .txt on d: drive \n"
    "    -!f *.txt d:       ; files NOT ending in .txt on d: drive \n" 
    "    -f *.txt -!f \\$RECY* d:  ; files ending in .txt but not in recyle.bin on d: drive \n" 
    "    -f F* c: d:        ; limit scan to files starting with F on either C or D \n"
    "\n"
    "  Filter using -g to specify regular expression\n"
    "    Example to filter on common executable extension changed less than 1 day ago\n"
    "    -g \".*[.](bat|ps|dll|com|exe)\" -T -t -1 c:\n"
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




// ------------------------------------------------------------------------------------------------
// Return negative on error, positive on success and zero if nothing done.

int wmain(int argc, const wchar_t* argv[])
{
    if (argc == 1)
    {
        std::wcout << sUsage;
        return 0;
    }

    WinErrHandlers::InitUnhandledExceptionFilter();

    bool loadUsnFromReg = false;
    bool matchOn = true;
    ReportCfg cfg;
    Ntfs ntfs;

    // dateFmt(L"dd-MMM-yyyy"), timeFmt(L"HH:mm"),
    Ntfs_Journal::ReadRegistry(L"TimeFormat", cfg.timeFmt);
    Ntfs_Journal::ReadRegistry(L"DateFormat", cfg.dateFmt);

    GetOpts<wchar_t> getOpts(argc, argv, L",!a:df:g:pr:s:t:u:AB:C:DF:R:STU?");
    const wchar_t* pArg;

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
            pArg = getOpts.OptArg();
            cfg.filter.List().push_back(new MatchName(std::wregex(pArg, std::regex::icase), IsGrepIcase, matchOn));
            break;

        case 'p':
            cfg.getFullPath = false;
            break;

        case 'r':
            cfg.reasonFilter = Ntfs_Journal::ParseReason(getOpts.OptArg());
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
            cfg.getFileLength = true;
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
                    cfg.reasonMergeAll = (pArg[0] == 'a');
                else
                    getOpts.NotOurArg();
            }
            break;
        case 'S':   // size
            cfg.size = !cfg.size;
            cfg.getFileLength = true;
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
                Ntfs_Journal::ReadRegistry(arg[0], cfg.startUsn);

            std::wcerr << L"--- Journal for " << arg << std::endl;

            DWORD tick = GetTickCount();
            error |= Ntfs_Journal::ListJournal(arg, ntfs, cfg);
            std::wcerr << L"--- " << (GetTickCount() - tick)/1000.0 << L" seconds\n";

            std::wcout << std::endl;
        }
    }

    if (ntfs.IsOpen() && ntfs.GetNextUsn() != 0)
    {
        Ntfs_Journal::WriteRegistry(ntfs.GetDrive(), ntfs.GetNextUsn());
    }

	return error;
}

