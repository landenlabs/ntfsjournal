#NTFSJournal

Windows only - Query NTFS journal

List various items stored in the NTFS journal database. 

Visit home website

[https://landenlabs.com](https://landenlabs.com)


Help Banner:
<pre>
Ntfs Journal  v3.1 - Dec 23 2024
By: Dennis Lang
https://landenlabs.com

Description:
  List NTFS Journal which tracks recent file/folder changes.
  Use 'fsutil usn ...' to create and configure NTFS journal.
Use:
   NtfsJournal [options] <localNTFSdrive>...
 Filter (see examples below):
   -a [d|f]                  ; Just Directories or Files, default is both
   -d                        ; Show detail, by default remove duplicates
   -f <findFilter>           ; Filter by file path, use * or ? patterns
   -g <findFilter>           ; Filter by file path, using grep reqular Expression ^[]+*.$
   -r <changeReasonFilter>   ; Filter by change flags
   -s <size>                 ; Filter by file size
   -t <relativeModifyDate>   ; Filter by Modify Time, value is relative days
   -u <usn>                  ; Start scan with usn number, see -U
   -u -                      ; Start with previously stored USN in registry
                             ; On exit, last USN is automatically stored in registry
 Report (what appears in output):
   -A                        ; Include attributes
   -B <dirAttr>              ; Change directory attribute 'D' to some other string
   -C <fmtChar>              ; Change format character '%' to some other character
   -D                        ; Disable directory
   -F <fmt>                  ; Format output, %t=time, %s=size, %r=reason,%a=attribute
                             ; %p=path(dir+filename), %c=drive, %d=directory,
                             ; %f=filename (name+ext), %n=name, %e=extension
                             ; Field can be padded, as in %10s %15t %20f
   -R [a|l]                  ; Include Reasons, All or just Last, default is just Last
   -S                        ; Include size
   -T                        ; Include modify time
   -U                        ; Include USN number

 Registry:
   HKEY_LOCAL_MACHINE\SOFTWARE\NtfsJournal
       TimeFormat  string   HH:mm          ; google 'msdn GetTimeFormat'
       DateFormat  string   dd-MMM-yyyy    ; google 'msdn GetDateFormat'

 Examples:
  No filtering:
    c:                 ; scan c drive, display filenames.
    -TSA c:            ; scan c drive, display  time, size, attributes.
  Filter examples (precede 'f' command letter with ! to invert rule):
    -f *.txt d:        ; files ending in .txt on d: drive
    -!f *.txt d:       ; files NOT ending in .txt on d: drive
    -f *.txt -!f \$RECY* d:  ; files ending in .txt but not in recyle.bin on d: drive
    -f F* c: d:        ; limit scan to files starting with F on either C or D

  Alternate using grep regular expression, note double backslash for every directory slash
  Also recommended you place the pattern inside quotations
    -g "\\tmp\\sub\\[^\\]+"   d:  ; files inside \tmp\sub\ but nothing deeper
  The above is similar to -f \tmp\sub\* except the -g version is anchored on the left
  and must match starting with its first character.

  Time and size options:
    -t 2.5 -f *.log    ; modified more than 2.5 days ago and ending in .log on c drive
    -t -7 e:           ; modified less than 7 days ago on e drive
    -s 1000 d:         ; size more than 1000 bytes on d drive
    -s -1000 d: e:     ; size less than 1000 bytes on d and e drive
                       ; *** NOTE: Size is rarely populated due to performance
    -F "%20t %20s %40p"  c: ; Format output
    -C # -F "#t,#s,#p"  c:  ; Change format character, and format output
    -F "copy %p \\remote\d$\data\%f" d:\data\* > sync.bat

  Filter Reasons Keywords:
       all,
       overwrite, extend, truncate
       create, delete, rename
       security, basic, link
  Examples:
     -r overwrite+extend+truncate  ; File content changes
     -r create+delete+rename       ; File life changes
   Defaults is:  overwrite+extend+truncate+create+delete+rename

</pre>
